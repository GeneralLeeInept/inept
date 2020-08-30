#include <gli.h>

class Bootstrap : public gli::App
{
public:
    bool on_create() override
    {
        return true;
    }

    void on_destroy() override
    {
    }

    bool on_update(float delta) override
    {
        return true;
    }
};

Bootstrap app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Bootstrap", 640, 360, 2))
    {
        app.run();
    }

    return 0;
}
