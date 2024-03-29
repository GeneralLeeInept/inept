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
    Key_LeftSystem,  // Windows / Super / Command
    Key_RightSystem, // Windows / Super / Command
    Key_PrintScreen,

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

enum MouseButton
{
    Left,
    Middle,
    Right
};

struct KeyEvent
{
    KeyEventType event;
    Key key;
    char ascii_code;
};

struct Pixel
{
    Pixel() = default;
    constexpr Pixel(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { argb = (a << 24) | (r << 16) | (g << 8) | b; }
    constexpr Pixel(uint32_t argb_in) { argb = argb_in; }

    Pixel operator*(float f);

    union
    {
        uint32_t argb = 0xFF000000;
        struct
        {
            uint8_t b;
            uint8_t g;
            uint8_t r;
            uint8_t a;
        };
    };

    static const uint32_t Black = 0xFF000000;
    static const uint32_t White = 0xFFFFFFFF;
    static const uint32_t Red = 0xFFFF0000;
    static const uint32_t Green = 0xFF00FF00;
    static const uint32_t Blue = 0xFF0000FF;
    static const uint32_t Clear = 0x00FFFFFF;
};

enum BlendMode
{
    One,
    Zero,
    SrcAlpha,
    InvSrcAlpha,
    DestAlpha,
    InvDestAlpha,
    Constant
};

enum BlendOp
{
    None,
    Multiply,
    Add,
    Subtract
};

class Sprite;

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
        int x; // Screen coords fixed for pixel scale
        int y;
        KeyState buttons[3]; // 0 = LMB, 1 = MMB, 2 = RMB
        int wheel; // Scroll wheel
    };

    struct ControllerState
    {
        float lx;
        float ly;
        float rx;
        float ry;
        float lt;
        float rt;
        KeyState buttons[16];
    };

    virtual ~App() = default;

    virtual bool on_create() = 0;
    virtual void on_destroy() = 0;
    virtual bool on_update(float delta) = 0;
    virtual void on_render(float delta); // FIXME - don't do this, do render layers

    bool initialize(const char* name, int screen_width_, int screen_height_, int window_scale);
    void run();
    void quit();

    int screen_width();
    int screen_height();

    const KeyState& key_state(Key key);
    const MouseState& mouse_state();
    void show_mouse(bool show);
    bool mouse_visible();
    ControllerState controller_state(int controller);

    using KeyEventHandler = std::function<void(KeyEvent&)>;
    void process_key_events(KeyEventHandler handler);

    void set_palette(uint32_t rgbx[256]);
    void set_palette(const uint8_t* palette, int size);
    void load_palette(const std::string& path);

    void clear_screen(uint8_t c);
    void set_pixel(int x, int y, uint8_t p);

    void set_blend_mode(BlendMode src_blend, BlendMode dest_blend, uint8_t constant);
    void set_blend_op(BlendOp op);

    void clear_screen(Pixel p);
    void set_pixel(int x, int y, Pixel p);

    void draw_line(int x1, int y1, int x2, int y2, uint8_t c);
    void draw_line(int x1, int y1, int x2, int y2, Pixel c);
    void draw_char(int x, int y, char c, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg);
    void draw_string(int x, int y, const char* str, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg);
    void draw_string(int x, int y, const char* str, const int* glyphs, int w, int h, Pixel fg, Pixel bg);
    void format_string(int x, int y, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg, const char* fmt, ...);
    void format_string(int x, int y, const int* glyphs, int w, int h, Pixel fg, Pixel bg, const char* fmt, ...);
    void draw_rect(int x, int y, int w, int h, uint8_t c);
    void fill_rect(int x, int y, int w, int h, int bw, uint8_t fg, uint8_t bg);
    void fill_rect(int x, int y, int w, int h, int bw, Pixel fg, Pixel bg);
    void copy_rect(int x, int y, int w, int h, const uint8_t* src, uint32_t stride);
    void copy_rect_scaled(int x, int y, int w, int h, const uint8_t* src, uint32_t stride, int pixel_scale);
    void draw_sprite(int x, int y, const Sprite* sprite);
    void draw_partial_sprite(int x, int y, const Sprite* sprite, int ox, int oy, int w, int h);
    void blend_sprite(int x, int y, const Sprite& sprite, uint8_t alpha);
    void blend_partial_sprite(int x, int y, const Sprite& sprite, int ox, int oy, int w, int h, uint8_t alpha);
    void set_screen_fade(Pixel color, float fade);

    Pixel* get_framebuffer();

    void request_screenshot(const std::string& directory);

    HWND get_window_handle() { return m_hwnd; }

private:
    void shutdown();
    void pump_messages();
    void engine_loop();
    void make_screenshot(const std::string& path);

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HWND m_hwnd = NULL;
    short* m_keystate[2] = {};
    int m_current_keystate = 0;
    Pixel* m_framebuffer = {};
    wchar_t* m_title = nullptr;
    Pixel m_palette[256] = {};
    KeyState m_keys[Key_Count] = {};
    MouseState m_mouse = {};
    int m_keymap[Key_Count] = {};
    int m_screen_width;
    int m_screen_height;
    int m_window_width;
    int m_window_height;
    Pixel m_fade_color;
    float m_fade;
    BlendMode m_src_blend = BlendMode::One;
    BlendMode m_dest_blend = BlendMode::Zero;
    uint8_t m_blend_constant = 255;
    BlendOp m_blend_op = BlendOp::None;
    bool m_screenshot_requested = false;
    std::string m_screenshot_directory{};
};

} // namespace gli
