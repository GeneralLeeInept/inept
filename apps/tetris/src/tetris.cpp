#include <gli.h>

class Tetris : public gli::App
{
public:
    bool on_create() override
    {
        return false;
    }

    void on_destroy() override
    {
    }

    bool on_update(float delta) override
    {
        return false;
    }
};

Tetris app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Tetris", 320, 240, 2))
    {
        app.run();
    }

    return 0;
}
