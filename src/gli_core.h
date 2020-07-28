#pragma once

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <fstream>
#include <functional>
#include <iterator>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace gli
{

enum Key
{
    Key_A,
    Key_B,
    Key_C,
    Key_D,
    Key_E,
    Key_F,
    Key_G,
    Key_H,
    Key_I,
    Key_J,
    Key_K,
    Key_L,
    Key_M,
    Key_N,
    Key_O,
    Key_P,
    Key_Q,
    Key_R,
    Key_S,
    Key_T,
    Key_U,
    Key_V,
    Key_W,
    Key_X,
    Key_Y,
    Key_Z,

    Key_0,
    Key_1,
    Key_2,
    Key_3,
    Key_4,
    Key_5,
    Key_6,
    Key_7,
    Key_8,
    Key_9,

    Key_Space,

    Key_BackTick,
    Key_Minus,
    Key_Equals,
    Key_LeftBracket,
    Key_RightBracket,
    Key_Backslash,
    Key_Semicolon,
    Key_Apostrophe,
    Key_Comma,
    Key_Period,
    Key_Slash,

    Key_Backspace,
    Key_Tab,
    Key_Enter,

    Key_Escape,
    Key_Menu,
    Key_LeftSystem, // Windows / Super / Command
    Key_RightSystem, // Windows / Super / Command

    Key_F1,
    Key_F2,
    Key_F3,
    Key_F4,
    Key_F5,
    Key_F6,
    Key_F7,
    Key_F8,
    Key_F9,
    Key_F10,
    Key_F11,
    Key_F12,

    Key_LeftShift,
    Key_RightShift,
    Key_LeftControl,
    Key_RightControl,
    Key_LeftAlt,
    Key_RightAlt,

    Key_Left,
    Key_Right,
    Key_Up,
    Key_Down,

    Key_PageUp,
    Key_PageDown,
    Key_Home,
    Key_End,

    Key_Insert,
    Key_Delete,

    Key_ScrollLock,
    Key_CapsLock,
    Key_NumLock,

    Key_Num_0,
    Key_Num_1,
    Key_Num_2,
    Key_Num_3,
    Key_Num_4,
    Key_Num_5,
    Key_Num_6,
    Key_Num_7,
    Key_Num_8,
    Key_Num_9,
    Key_Num_Add,
    Key_Num_Subtract,
    Key_Num_Multiply,
    Key_Num_Divide,
    Key_Num_Decimal,
    Key_Num_Enter,

    Key_Count
};

enum KeyEventType
{
    Pressed,
    Released,
    Repeat
};

struct KeyEvent
{
    KeyEventType event;
    Key key;
    char ascii_code;
};

class App
{
public:
    struct KeyState
    {
        bool pressed;
        bool released;
        bool down;
    };

    struct MouseState
    {
        int x;                  // Screen coords fixed for scale
        int y;
        KeyState buttons[3];    // 0 = LMB, 1 = MMB, 2 = RMB
    };

    virtual ~App() = default;

    virtual bool on_create() = 0;
    virtual void on_destroy() = 0;
    virtual bool on_update(float delta) = 0;

    bool initialize(const char* name, int screen_width_, int screen_height_, int window_scale);
    void run();

    int screen_width();
    int screen_height();

    const KeyState& key_state(Key key);
    const MouseState& mouse_state();

    using KeyEventHandler = std::function<void(KeyEvent&)>;
    void process_key_events(KeyEventHandler handler);

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
    KeyState m_keys[Key_Count] = {};
    MouseState m_mouse = {};
    int m_keymap[Key_Count] = {};
    int m_screen_width;
    int m_screen_height;
    int m_window_width;
    int m_window_height;
};

}
