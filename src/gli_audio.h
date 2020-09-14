#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gli
{

class RingBuffer
{
public:
    void init(size_t capacity, bool start_full);
    void reset(bool start_full);

    size_t capacity() const;
    size_t size() const;

    void lock(float*& ptr1, size_t& len1, float*& ptr2, size_t& len2, size_t length);
    void unlock(size_t length);

    size_t read(float* buffer, size_t length);

private:
    mutable std::recursive_mutex _mutex;
    std::vector<float> _data;
    size_t _head;
    size_t _tail;
    bool _full;
};


class WaveForm
{
public:
    bool load(const std::string& path);

    size_t num_channels() const;
    size_t length() const;
    size_t read(float* data, size_t position, size_t num_frames, size_t loopcount) const;

private:
    size_t _num_channels{};
    size_t _length{};
    std::vector<float> _samples;
};


class AudioSource
{
public:
    virtual ~AudioSource() = default;

    virtual size_t num_channels() const = 0;
    virtual bool finished() const = 0;

    virtual void reset() = 0;
    virtual size_t read(float* data, size_t num_frames) = 0;

    virtual float get_fade() const { return _fade; }
    virtual void set_fade(float fade) { _fade = fade; }

protected:
    float _fade{ 1.0f };
};


class SubMix : public AudioSource
{
public:
    void add_source(std::unique_ptr<AudioSource> source);

    size_t num_channels() const override;
    bool finished() const override;

    void reset() override;
    size_t read(float* data, size_t num_frames) override;

protected:
    std::vector<std::unique_ptr<AudioSource>> _inputs;
};


class Channel : public AudioSource
{
public:
    static const size_t LoopInfinite = (size_t)-1;

    Channel(const WaveForm& waveform);

    void set_loop_count(size_t loopcount);

    size_t num_channels() const override;
    bool finished() const override;

    void reset() override;
    size_t read(float* data, size_t num_frames) override;

private:
    const WaveForm& _waveform;
    size_t _position{};
    size_t _loopcount{};
    bool _finished{ false };
};


class AudioEngine
{
public:
    AudioEngine() = default;
    ~AudioEngine();

    bool init();
    void shutdown();

    bool start();
    void stop();

    void update();

    void play_sound(const WaveForm& waveform, float fade, size_t loopcount);

private:
    struct WasapiState;

    struct WasapiStateDeleter
    {
        void operator()(WasapiState*) const;
    };

    std::unique_ptr<WasapiState, WasapiStateDeleter> _wasapi;
    SubMix _master;
};

} // namespace gli
