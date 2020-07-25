#include "gli.h"
#include "vga9.h"

class TestApp : public gli::App
{
public:
    bool on_create() override
    {
        return true;
    }

    void on_destroy() override
    {
    }

    uint8_t clear_color = 0;
    float next_color = 0.25f;

    bool on_update(float delta) override
    {
        next_color -= delta;

        while (next_color < 0.0f)
        {
            clear_color++;
            next_color += 0.25f;
        }

        clear_screen(clear_color);
        draw_line(10, 10, 100, 100, 255 - clear_color);
        draw_line(11, 10, 101, 100, 255 - clear_color);
        draw_line(12, 10, 102, 100, 255 - clear_color);
        format_string(10, 120, vga9_glyphs, vga9_glyph_width, vga9_glyph_height, 255 - clear_color, clear_color, "Hello, %s!", "World");
        return true;
    }
};


int gli_main(int argc, char** argv)
{
    TestApp test_app;
    test_app.initialize("Hello World", 320, 240, 4);
    test_app.run();
    return 0;
}