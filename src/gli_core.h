#pragma once

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <fstream>
#include <iterator>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace gli
{

class App
{
public:
    int screen_width;
    int screen_height;
    int window_width;
    int window_height;

    virtual ~App() = default;

    virtual bool on_create() = 0;
    virtual void on_destroy() = 0;
    virtual bool on_update(float delta) = 0;

    bool initialize(const char* name, int screen_width_, int screen_height_, int window_scale);
    void run();

    void set_palette(uint32_t rgbx[256]);
    void set_palette(const uint8_t* palette, int size);
    void load_palette(const std::string& path);

    void clear_screen(uint8_t c);
    void set_pixel(int x, int y, uint8_t p);


    void draw_line(int x1, int y1, int x2, int y2, uint8_t c);
    void draw_char(int x, int y, char c, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg);
    void draw_string(int x, int y, const char* str, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg);
    void format_string(int x, int y, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg, const char* fmt, ...);
    void draw_rect(int x, int y, int w, int h, uint8_t c);
    void fill_rect(int x, int y, int w, int h, int bw, uint8_t fg, uint8_t bg);
    void copy_rect(int x, int y, int w, int h, const uint8_t* src, uint32_t stride);
    void copy_rect_scaled(int x, int y, int w, int h, const uint8_t* src, uint32_t stride, int pixel_scale);

protected:
    struct KeyState
    {
        bool pressed;
        bool released;
        bool down;
    };

    KeyState m_keys[256] = {};

private:
    void shutdown();
    void pump_messages();
    void engine_loop();

    static LRESULT window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd = NULL;
    short* m_keystate[2] = {};
    int m_current_keystate = 0;
    uint8_t* m_framebuffer = {};
    wchar_t* m_title = nullptr;
    RGBQUAD m_palette[256] = {};
};

}
