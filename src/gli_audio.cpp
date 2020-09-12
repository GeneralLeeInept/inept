#include "gli_audio.h"

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

#define wasapi_check(result, func, ...)                             \
    do                                                              \
    {                                                               \
        HRESULT hr = (result);                                      \
        if (!SUCCEEDED(hr))                                         \
        {                                                           \
            gliLog(LogLevel::Error, "Audio", func, __VA_ARGS__);    \
            return false;                                           \
        }                                                           \
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
    if (stop_handle)
    {
        SetEvent(stop_handle);
    }

    if (output_thread.joinable())
    {
        output_thread.join();
    }

    if (render_client)
    {
        render_client->Release();
    }

    if (audio_client)
    {
        audio_client->Release();
    }

    if (stop_handle)
    {
        CloseHandle(stop_handle);
    }

    if (event_handle)
    {
        CloseHandle(event_handle);
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
    wasapi_check(device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&audio_client), "WasapiState::init",
                 "IMMDevice::Activate failed.");
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

    gliLog(LogLevel::Info, "Audio", "WasapiState::init", "Audio initialized. Output sample rate %uHz, channels %d.", sample_rate, num_channels);

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
        wasapi_check(audio_client->GetBufferSize(&buffer_size), "WasapiState::start", "IAudioClient::GetBufferSize failed.");

        float* out_buffer;
        wasapi_check(render_client->GetBuffer(buffer_size, (BYTE**)&out_buffer), "WasapiState::start",
                     "IAudioRenderClient::GetBuffer failed.");
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
                float* out_buffer;
                hr = render_client->GetBuffer(frames_required, reinterpret_cast<BYTE**>(&out_buffer));

                if (SUCCEEDED(hr))
                {
                    render_client->ReleaseBuffer(frames_required, AUDCLNT_BUFFERFLAGS_SILENT);
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

} // namespace gli
