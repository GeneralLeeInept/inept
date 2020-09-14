#include "gli_audio.h"

#include "gli_debug.h"
#include "gli_file.h"
#include "gli_log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <ksmedia.h>

#include <cstdint>
#include <thread>

namespace gli
{

#define wasapi_check(result, func, ...)                          \
    do                                                           \
    {                                                            \
        HRESULT hr = (result);                                   \
        if (!SUCCEEDED(hr))                                      \
        {                                                        \
            gliLog(LogLevel::Error, "Audio", func, __VA_ARGS__); \
            return false;                                        \
        }                                                        \
    } while (0)


template <typename T>
struct AutoComInterfaceDeleter
{
    void operator()(T* ptr) const
    {
        if (ptr) ptr->Release();
    }
};


template <typename T>
using AutoComInterface = std::unique_ptr<T, AutoComInterfaceDeleter<T>>;


template <typename T>
struct AutoComPtrDeleter
{
    void operator()(T* ptr) const
    {
        if (ptr) CoTaskMemFree(ptr);
    }
};


template <typename T>
using AutoComPtr = std::unique_ptr<T, AutoComPtrDeleter<T>>;


struct AudioEngine::WasapiState
{
    IAudioClient* audio_client{};
    IAudioRenderClient* render_client{};
    uint32_t buffer_size{};
    uint32_t sample_rate{};
    uint32_t num_channels{};
    HANDLE event_handle{};
    HANDLE stop_handle{};
    std::thread output_thread{};
    RingBuffer output_buffer{};
    SubMix* _master;

    ~WasapiState();

    bool init();
    bool start();
    void stop();

    void output_thread_func();
};


void AudioEngine::WasapiStateDeleter::operator()(WasapiState* state) const
{
    delete state;
}


AudioEngine::WasapiState::~WasapiState()
{
    if (output_thread.joinable()) 
    {
        stop();
    }

    if (render_client)
    {
        render_client->Release();
        render_client = nullptr;
    }

    if (audio_client)
    {
        audio_client->Release();
        audio_client = nullptr;
    }

    if (stop_handle)
    {
        CloseHandle(stop_handle);
        stop_handle = 0;
    }

    if (event_handle)
    {
        CloseHandle(event_handle);
        event_handle = 0;
    }
}


bool AudioEngine::WasapiState::init()
{
    CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    IMMDeviceEnumerator* device_enumerator_handle{};
    wasapi_check(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&device_enumerator_handle),
                 "WasapiState::init", "Failed to create device enumerator.");

    AutoComInterface<IMMDeviceEnumerator> device_enumerator{ device_enumerator_handle };
    IMMDevice* device_handle{};
    wasapi_check(device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_handle), "WasapiState::init",
                 "IMMDeviceEnumerator::GetDefaultAudioEndpoint failed.");
    device_enumerator.reset();

    AutoComInterface<IMMDevice> device{ device_handle };
    IID IID_IAudioClient = __uuidof(IAudioClient);
    wasapi_check(device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&audio_client), "WasapiState::init", "IMMDevice::Activate failed.");
    device.reset();

    WAVEFORMATEX* device_format_memory{};
    wasapi_check(audio_client->GetMixFormat(&device_format_memory), "WasapiState::init", "IAudioRenderClient::GetMixFormat failed.");

    AutoComPtr<WAVEFORMATEX> device_format{ device_format_memory };
    wasapi_check(audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, device_format.get(), nullptr),
                 "WasapiState::init", "IAudioRenderClient::Initialize failed.");

    IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
    wasapi_check(audio_client->GetService(IID_IAudioRenderClient, (void**)&render_client), "WasapiState::init",
                 "IAudioRenderClient::GetService failed.");

    event_handle = CreateEventA(0, 0, 0, 0);

    if (!event_handle)
    {
        gliLog(LogLevel::Error, "Audio", "WasapiState::init", "CreateEvent failed.");
        return false;
    }

    stop_handle = CreateEventA(0, 0, 0, 0);

    if (!stop_handle)
    {
        gliLog(LogLevel::Error, "Audio", "WasapiState::init", "CreateEvent failed.");
        return false;
    }

    wasapi_check(audio_client->SetEventHandle(event_handle), "WasapiState::init", "IAudioRenderClient::SetEventHandle failed.");

    sample_rate = device_format->nSamplesPerSec;
    num_channels = device_format->nChannels;

    wasapi_check(audio_client->GetBufferSize(&buffer_size), "WasapiState::start", "IAudioClient::GetBufferSize failed.");

    gliLog(LogLevel::Info, "Audio", "WasapiState::init", "Audio initialized. Output sample rate %uHz, channels %d, buffer size %d.", sample_rate, num_channels, buffer_size);

    return true;
}


bool AudioEngine::WasapiState::start()
{
    bool result = false;

    if (output_thread.joinable())
    {
        gliLog(LogLevel::Error, "Audio", "WasapiState::start", "Already started.");
    }
    else
    {
        output_buffer.init((size_t)1024 * num_channels, true);

        wasapi_check(audio_client->GetBufferSize(&buffer_size), "WasapiState::start", "IAudioClient::GetBufferSize failed.");

        float* buffer;
        wasapi_check(render_client->GetBuffer(buffer_size, (BYTE**)&buffer), "WasapiState::start", "IAudioRenderClient::GetBuffer failed.");
        wasapi_check(render_client->ReleaseBuffer(buffer_size, AUDCLNT_BUFFERFLAGS_SILENT), "WasapiState::start",
                     "IAudioRenderClient::ReleaseBuffer failed.");
        wasapi_check(audio_client->Start(), "WasapiState::start", "IAudioClient::Start failed.");

        ResetEvent(event_handle);
        ResetEvent(stop_handle);

        output_thread = std::move(std::thread(&AudioEngine::WasapiState::output_thread_func, this));
        result = output_thread.joinable();
    }

    return result;
}


void AudioEngine::WasapiState::stop()
{
    if (output_thread.joinable())
    {
        SetEvent(stop_handle);
        output_thread.join();
        HRESULT hr = audio_client->Stop();

        if (FAILED(hr))
        {
            gliLog(LogLevel::Warning, "Audio", "WasapiState::stop", "IAudioClient::Start failed.");
        }
    }
    else
    {
        gliLog(LogLevel::Warning, "Audio", "WasapiState::stop", "Already stopped.");
    }
}


void AudioEngine::WasapiState::output_thread_func()
{
    HANDLE wait_handles[2] = { event_handle, stop_handle };

    while (WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE) == WAIT_OBJECT_0)
    {
        uint32_t current_padding;
        HRESULT hr = audio_client->GetCurrentPadding(&current_padding);

        if (SUCCEEDED(hr))
        {
            uint32_t frames_required = buffer_size - current_padding;

            if (frames_required)
            {
                float* buffer;
                hr = render_client->GetBuffer(frames_required, reinterpret_cast<BYTE**>(&buffer));

                if (SUCCEEDED(hr))
                {
                    #if 1
                        size_t frames_available = output_buffer.size() / num_channels;

                        if (frames_available > frames_required)
                        {
                            frames_available = frames_required;
                        }

                        if (frames_available)
                        {
                            output_buffer.read(buffer, frames_available * num_channels);
                            render_client->ReleaseBuffer((UINT32)frames_available, 0);
                        }
                        else
                        {
                            gliLog(LogLevel::Warning, "Audio", "WasapiState::output_thread_func", "Output buffer underrun.");
                            render_client->ReleaseBuffer(frames_required, AUDCLNT_BUFFERFLAGS_SILENT);
                        }
                    #else
                        _master->read(buffer, frames_required);
                        render_client->ReleaseBuffer(frames_required, 0);
                    #endif
                }
                else
                {
                    gliLog(LogLevel::Warning, "Audio", "WasapiState::output_thread_func", "GetBuffer failed.");
                }
            }
        }
        else
        {
            gliLog(LogLevel::Warning, "Audio", "WasapiState::output_thread_func", "GetCurrentPadding failed.");
        }
    }
}


bool AudioEngine::init()
{
    bool result = false;

    if (!_wasapi)
    {
        std::unique_ptr<WasapiState, WasapiStateDeleter> wasapi_state = std::unique_ptr<WasapiState, WasapiStateDeleter>(new WasapiState());
        result = wasapi_state->init();

        if (result)
        {
            _wasapi = std::move(wasapi_state);
            _wasapi->_master = &_master;
        }
    }

    return result;
}


AudioEngine::~AudioEngine() = default;


void AudioEngine::shutdown()
{
    stop();
    _wasapi = nullptr;
}


bool AudioEngine::start()
{
    bool result = false;

    if (_wasapi)
    {
        result = _wasapi->start();
    }

    return result;
}


void AudioEngine::stop()
{
    if (_wasapi)
    {
        _wasapi->stop();
    }
}


void AudioEngine::update()
{
    if (_wasapi)
    {
        float* mix1;
        size_t len1;
        float* mix2;
        size_t len2;

        _wasapi->output_buffer.lock(mix1, len1, mix2, len2, (size_t)-1);

        if (len1)
        {
            _master.read(mix1, len1 / 2);

            if (len2)
            {
                _master.read(mix2, len2 / 2);
            }
        }

        _wasapi->output_buffer.unlock(len1 + len2);
    }
}


void AudioEngine::play_sound(const WaveForm& waveform, float fade, size_t loopcount)
{
    std::unique_ptr<Channel> channel = std::make_unique<Channel>(waveform);
    channel->set_fade(fade);
    channel->set_loop_count(loopcount);
    _master.add_source(std::move(channel));
}


void RingBuffer::init(size_t capacity, bool start_full)
{
    _data.resize(capacity, 0.0f);
    _head = 0;
    _tail = 0;
    _full = start_full;
}


void RingBuffer::reset(bool start_full)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _head = 0;
    _tail = 0;
    _full = start_full;

    if (start_full)
    {
        memset(&_data[0], 0, capacity());
    }
}


size_t RingBuffer::capacity() const
{
    return _data.size();
}


size_t RingBuffer::size() const
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    size_t s = capacity();

    if (!_full)
    {
        if (_tail >= _head)
        {
            s = _tail - _head;
        }
        else
        {
            s = s - _head + _tail;
        }
    }

    return s;
}


void RingBuffer::lock(float*& ptr1, size_t& len1, float*& ptr2, size_t& len2, size_t length)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    size_t free_space = _full ? 0 : (capacity() - size());

    if (free_space < length || length == (size_t)-1)
    {
        length = free_space;
    }

    ptr1 = &_data[_tail];

    if (_tail + length < capacity())
    {
        len1 = length;
        ptr2 = nullptr;
        len2 = 0;
    }
    else
    {
        len1 = capacity() - _tail;
        ptr2 = &_data[0];
        len2 = length - len1;
    }
}


void RingBuffer::unlock(size_t length)
{
    if (length)
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _tail = (_tail + length) % capacity();
        _full = (_tail == _head);
    }
}


size_t RingBuffer::read(float* buffer, size_t length)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (length > size())
    {
        length = size();
    }

    size_t len1 = length;
    size_t len2 = 0;

    if (_head + length > capacity())
    {
        len1 = capacity() - _head;
        len2 = length - len1;
    }

    memcpy(buffer, &_data[_head], len1 * sizeof(float));

    if (len2)
    {
        memcpy(buffer + len1, &_data[0], len2 * sizeof(float));
    }

    _head = (_head + length) % capacity();
    _full = false;

    return length;
}


static inline constexpr uint32_t fourcc(const char* s)
{
    return ((s[3] << 24) | (s[2] << 16) | (s[1] << 8) | (s[0] << 0));
}


bool WaveForm::load(const std::string& path)
{
    std::vector<uint8_t> data;

    if (!FileSystem::get()->read_entire_file(path.c_str(), data))
    {
        return false;
    }

    uint8_t* fileptr = (uint8_t*)&data[0];
    size_t length = data.size();

    struct ChunkHeader
    {
        uint32_t id;
        uint32_t size;
    };

    struct RiffHeader : ChunkHeader
    {
        uint32_t format;
    };

    if (length < sizeof(RiffHeader))
    {
        return false;
    }

    RiffHeader header;
    memcpy(&header, fileptr, sizeof(RiffHeader));
    fileptr += sizeof(RiffHeader);
    length -= sizeof(RiffHeader);

    if (header.id != fourcc("RIFF"))
    {
        return false;
    }

    if (header.format != fourcc("WAVE"))
    {
        return false;
    }

    struct WaveFormat : ChunkHeader
    {
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
    };

    if (length < sizeof(WaveFormat))
    {
        return false;
    }

    WaveFormat fmt{};
    memcpy(&fmt, fileptr, sizeof(WaveFormat));

    if (fmt.id != fourcc("fmt "))
    {
        return false;
    }

    fileptr += sizeof(ChunkHeader) + fmt.size;
    length -= sizeof(ChunkHeader) + fmt.size;

    if (fmt.audio_format != 1 || fmt.bits_per_sample != 16 || fmt.sample_rate != 48000)
    {
        return false;
    }

    if (length < sizeof(ChunkHeader))
    {
        return false;
    }

    ChunkHeader data_header;
    memcpy(&data_header, fileptr, sizeof(ChunkHeader));

    if (data_header.id != fourcc("data"))
    {
        return false;
    }

    fileptr += sizeof(ChunkHeader);
    length -= sizeof(ChunkHeader);

    if (length < data_header.size)
    {
        return false;
    }

    _num_channels = (size_t)fmt.num_channels;
    _length = (size_t)data_header.size / (size_t)fmt.block_align;
    _samples.reserve(_num_channels * _length);
    static const float convp = 1.0f / (float)std::numeric_limits<int16_t>::max();
    static const float convn = -1.0f / (float)std::numeric_limits<int16_t>::min();

    for (size_t f = 0; f < _length; ++f)
    {
        int16_t* sample_ptr = (int16_t*)fileptr;

        for (size_t c = 0; c < _num_channels; ++c)
        {
            int16_t samplei = sample_ptr[c];
            float sample = (float)samplei / (float)INT16_MAX;// * (samplei < 0 ? convn : convp);
            _samples.push_back(sample);
        }

        fileptr += fmt.block_align;
    }

    return true;
}


size_t WaveForm::num_channels() const
{
    return _num_channels;
}


size_t WaveForm::length() const
{
    return _length;
}


size_t WaveForm::read(float* buffer, size_t position, size_t num_frames, size_t loopcount) const
{
    size_t frames_read = 0;
    size_t end_pos = (loopcount == Channel::LoopInfinite) ? (position + num_frames) : ((loopcount + 1) * _length);

    if (position + num_frames > end_pos)
    {
        num_frames = end_pos - position;
    }

    while (num_frames)
    {
        size_t start_pos = position % _length;
        size_t frames_to_read = 0;

        if (start_pos + num_frames > _length)
        {
            frames_to_read = _length - start_pos;
        }
        else
        {
            frames_to_read = num_frames;
        }

        if (frames_to_read)
        {
            memcpy(buffer, &_samples[start_pos * _num_channels], frames_to_read * _num_channels * sizeof(float));
            num_frames -= frames_to_read;
            frames_read += frames_to_read;
            buffer += frames_to_read * _num_channels;
        }
        else
        {
            gliAssert(frames_to_read);
            break;
        }
    }

    return frames_read;
}

Channel::Channel(const WaveForm& waveform)
    : _waveform(waveform)
{
}


void Channel::set_loop_count(size_t loopcount)
{
    _loopcount = loopcount;
}

size_t Channel::num_channels() const
{
    return _waveform.num_channels();
}

bool Channel::finished() const
{
    return _finished;
}

void Channel::reset()
{
    _position = 0;
    _finished = false;
}

size_t Channel::read(float* data, size_t num_frames)
{
    size_t frames_read = _waveform.read(data, _position, num_frames, _loopcount);
    _finished = (frames_read < num_frames);
    _position += frames_read;
    return frames_read;
}

void SubMix::add_source(std::unique_ptr<AudioSource> source)
{
    _inputs.push_back(std::move(source));
}

size_t SubMix::num_channels() const
{
    return 2;
}

bool SubMix::finished() const
{
    return false;
}

void SubMix::reset() {}

size_t SubMix::read(float* data, size_t num_frames)
{
    memset(data, 0, num_frames * num_channels() * sizeof(float));
    std::vector<float> temp(num_frames * num_channels());

    for (auto itr = _inputs.begin(); itr != _inputs.end(); )
    {
        size_t frames_read = (*itr)->read(&temp[0], num_frames);

        if ((*itr)->num_channels() == 2)
        {
            float fade = (*itr)->get_fade();
            float* src = &temp[0];
            float* dest = data;

            for (size_t f = 0; f < frames_read; ++f)
            {
                for (size_t c = 0; c < num_channels(); ++c)
                {
                    dest[c] += src[c] * fade;
                }

                dest += num_channels();
                src += num_channels();
            }
        }
        else
        {
            static const float half_sqrt2 = 0.70710678f;
            float fade = (*itr)->get_fade() * half_sqrt2;
            float* src = &temp[0];
            float* dest = data;

            for (size_t f = 0; f < frames_read; ++f)
            {
                for (size_t c = 0; c < num_channels(); ++c)
                {
                    dest[c] += src[0] * fade;
                }

                dest += num_channels();
                src++;
            }
        }

        if ((*itr)->finished())
        {
            itr = _inputs.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    return num_frames;
}

} // namespace gli
