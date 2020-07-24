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

    void set_pixel(int x, int y, uint8_t p)
    {
        if (x >= 0 && x < screen_width && y >= 0 && y < screen_height)
        {
            uint8_t* backbuffer = m_framebuffer[m_frontbuffer ^ 1];
            backbuffer[x + y * screen_width] = p;
        }
    }


    uint8_t get_pixel(int x, int y)
    {
        uint8_t p = 0;

        if (x >= 0 && x < screen_width && y >= 0 && y < screen_height)
        {
            uint8_t* backbuffer = m_framebuffer[m_frontbuffer ^ 1];
            p = backbuffer[x + y * screen_width];
        }

        return p;
    }


    void set_palette(uint32_t rgbx[256])
    {
        for (int p = 0; p < 256; ++p)
        {
            RGBQUAD entry = {};
            entry.rgbRed = (rgbx[p] >> 16) & 255;
            entry.rgbGreen = (rgbx[p] >> 8) & 255;
            entry.rgbBlue = rgbx[p] & 255;
            m_palette[p] = entry;
        }
    }


    void set_palette(const uint8_t* palette, int size)
    {
        RGBQUAD* dest = m_palette;

        for (int p = 0; p < size; p += 3)
        {
            dest->rgbRed = palette[p];
            dest->rgbGreen = palette[p + 1];
            dest->rgbBlue = palette[p + 2];
            dest++;
        }
    }


    void load_palette(const std::string& path)
    {
        uint8_t palette[768];
        memset(palette, 0, sizeof(palette));
        std::ifstream f(path, std::ios::binary);

        if (f)
        {
            f.read((char*)palette, 768);
        }

        set_palette(palette, 768);
    }


    void clear_screen(uint8_t c)
    {
        uint8_t* backbuffer = m_framebuffer[m_frontbuffer ^ 1];
        size_t buffer_size = size_t(screen_width) * size_t(screen_height);
        memset(backbuffer, c, buffer_size);
    }


    void draw_line(int x1, int y1, int x2, int y2, uint8_t c)
    {
        int delta_x = x2 - x1;
        int delta_y = y2 - y1;
        int step_x = delta_x > 0 ? 1 : (delta_x < 0 ? -1 : 0);
        int step_y = delta_y > 0 ? 1 : (delta_y < 0 ? -1 : 0);

        if ((delta_x * step_x) > (delta_y * step_y))
        {
            // x-major
            int y = y1;
            int error = 0;

            for (int x = x1; x != x2; x += step_x)
            {
                set_pixel(x, y, c);

                error += step_y * delta_y;

                if ((error * 2) >= step_x * delta_x)
                {
                    y += step_y;
                    error -= step_x * delta_x;
                }
            }
        }
        else
        {
            // y-major
            int x = x1;
            int error = 0;

            for (int y = y1; y != y2; y += step_y)
            {
                set_pixel(x, y, c);

                error += step_x * delta_x;

                if ((error * 2) >= step_y * delta_y)
                {
                    x += step_x;
                    error -= step_y * delta_y;
                }
            }
        }
    }


    void draw_char(int x, int y, char c, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg)
    {
        uint8_t colors[2] = { bg, fg };
        const int* glyph = glyphs + c * w * h;

        for (int py = 0; py < h; ++py)
        {
            for (int px = 0; px < w; ++px)
            {
                set_pixel(x + px, y + py, colors[glyph[py * w + px]]);
            }
        }
    }


    void draw_string(int x, int y, const char* str, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg)
    {
        uint8_t colors[2] = { bg, fg };

        for (const char* c = str; *c; ++c)
        {
            const int* glyph = glyphs + *c * w * h;

            for (int py = 0; py < h; ++py)
            {
                for (int px = 0; px < w; ++px)
                {
                    set_pixel(x + px, y + py, colors[glyph[py * w + px]]);
                }
            }

            x += w;
        }
    }


    void format_string(int x, int y, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(nullptr, 0, fmt, args) + 1;
        va_end(args);

        char* buf = (char*)_malloca(len);

        if (!buf)
        {
            return;
        }

        va_start(args, fmt);
        vsnprintf(buf, len, fmt, args);
        va_end(args);

        draw_string(x, y, buf, glyphs, w, h, fg, bg);

        _freea(buf);
    }


    void draw_rect(int x, int y, int w, int h, uint8_t c)
    {
        for (int px = 0; px < w; ++px)
        {
            set_pixel(x + px, y, c);
            set_pixel(x + px, y + h - 1, c);
        }

        for (int py = 0; py < h; ++py)
        {
            set_pixel(x, y + py, c);
            set_pixel(x + w - 1, y + py, c);
        }
    }


    void fill_rect(int x, int y, int w, int h, int bw, uint8_t fg, uint8_t bg)
    {
        uint8_t colors[2] = { fg, bg };

        for (int py = 0; py < h; ++py)
        {
            for (int px = 0; px < w; ++px)
            {
                int color_index = (px < bw || px > w - 1 - bw || py < bw || py > h - 1 - bw);
                set_pixel(x + px, y + py, colors[color_index]);
            }
        }
    }


    void copy_rect(int x, int y, int w, int h, const uint8_t* src, uint32_t stride)
    {
        uint8_t* backbuffer = m_framebuffer[m_frontbuffer ^ 1];
        uint8_t* dest = backbuffer + x + y * screen_width;

        x = (x + w < screen_width) ? x : (screen_width - x);

        if (x <= 0) return;

        y = (y + h < screen_height) ? y : (screen_height - y);

        if (y <= 0) return;

        for (int y = 0; y < h; ++y)
        {
            memcpy(dest, src, w);
            dest += screen_width;
            src += stride;
        }
    }


    void copy_rect_scaled(int x, int y, int w, int h, const uint8_t* src, uint32_t stride, int pixel_scale)
    {
        uint8_t* backbuffer = m_framebuffer[m_frontbuffer ^ 1];
        uint8_t* dest = backbuffer + x + y * screen_width;

        w = (x + w < screen_width) ? w : (screen_width - x);

        if (x <= 0) return;

        h = (y + h < screen_height) ? h : (screen_height - y);

        if (y <= 0) return;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                dest[x] = src[(x / pixel_scale) + ((y / pixel_scale) * stride)];
            }

            dest += screen_width;
        }
    }


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
    uint8_t* m_framebuffer[2] = {};
    int m_frontbuffer = 0;
    wchar_t* m_title = nullptr;
    RGBQUAD m_palette[256] = {};
};

}