#include <gli.h>

class ${Template} : public gli::App
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

${Template} app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("${Template}", 320, 240, 2))
    {
        app.run();
    }

    return 0;
}
