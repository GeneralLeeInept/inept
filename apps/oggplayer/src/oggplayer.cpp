#include <gli.h>

class OggPlayer : public gli::App
{
public:
    gli::AudioEngine audio_engine;
    gli::OggFile ogg_file;

    bool on_create() override
    {
        if (!(audio_engine.init() && audio_engine.start()))
        {
            return false;
        }

        if (!ogg_file.open(R"(R:\assets\sounds\median_test.ogg)"))
        {
            return false;
        }

        audio_engine.play_sound(ogg_file, 1.f, gli::Sound::LoopInfinite);

        return true;
    }

    void on_destroy() override
    {
    }

    bool on_update(float delta) override
    {
        audio_engine.update();
        return true;
    }
};

OggPlayer app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("OggPlayer", 320, 240, 2))
    {
        app.run();
    }

    return 0;
}
