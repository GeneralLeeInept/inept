#pragma once

#include <memory>

namespace gli
{

class AudioEngine
{
public:
    AudioEngine() = default;
    ~AudioEngine();

    bool init();
    void shutdown();

    bool start();
    void stop();

private:
    struct WasapiState;

    struct WasapiStateDeleter
    {
        void operator()(WasapiState*) const;
    };

    std::unique_ptr<WasapiState, WasapiStateDeleter> _wasapi;
};

} // namespace gli
