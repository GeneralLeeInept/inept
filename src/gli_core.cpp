#include "gli_core.h"

#include "gli_log.h"
#include "gli_opengl.h"
#include "gli_sprite.h"

#include "opengl40/glad.h"

#include <objbase.h>

#include <atomic>
#include <thread>
#include <unordered_map>


extern int gli_main(int argc, char** argv);

static const char* s_shader_source[2]{
    R"(
#version 400 core

in vec2 vPos;
out vec2 vTexCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(vPos, 0, 1);
    vTexCoord = (vPos + 1.0) * vec2(0.5, -0.5);
}
    )",

    R"(
#version 400 core

uniform sampler2D sDiffuseMap;

in vec2 vTexCoord;
out vec4 oColor;

void main()
{
    oColor = texture(sDiffuseMap, vTexCoord);
}
    )"
};

namespace gli
{

char* wchar_to_utf8(const wchar_t* mbcs);
wchar_t* utf8_to_wchar(const char* utf8);
OpenGL _opengl;
GLuint _shader_program{};
GLuint _vbo = 0;
GLuint _vao = 0;
GLuint _texture = 0;
std::atomic<bool> _quit;
std::atomic<bool> _active;
std::atomic<bool> _mouse_active;
std::atomic<App::MouseState> _mouse_state;

bool App::initialize(const char* name, int screen_width, int screen_height, int window_scale)
{
    if (screen_width <= 0 || screen_height <= 0 || window_scale <= 0)
    {
        return false;
    }

    m_title = utf8_to_wchar(name);

    m_screen_width = screen_width;
    m_screen_height = screen_height;
    m_window_width = screen_width * window_scale;
    m_window_height = screen_height * window_scale;

    // Create window
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"IneptEngineWindow";

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

    DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    RECT client_rect = { 0, 0, m_window_width, m_window_height };
    AdjustWindowRectEx(&client_rect, dwStyle, FALSE, dwExStyle);
    int width = client_rect.right - client_rect.left;
    int height = client_rect.bottom - client_rect.top;
    m_hwnd = CreateWindowEx(dwExStyle, L"IneptEngineWindow", m_title, dwStyle, 0, 0, width, height, NULL, NULL, GetModuleHandle(NULL), NULL);
    SetProp(m_hwnd, L"IneptEngineWindow", (HANDLE)this);

    if (m_hwnd == NULL)
    {
        return false;
    }

    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    SetWindowPos(m_hwnd, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    // Initialise key states
    for (int i = 0; i < 2; ++i)
    {
        m_keystate[i] = new short[256];
        memset(m_keystate[i], 0, sizeof(short) * 256);
    }

    // Map of gli::Key codes to Windows virtual keys
    m_keymap[Key_A] = 'A';
    m_keymap[Key_B] = 'B';
    m_keymap[Key_C] = 'C';
    m_keymap[Key_D] = 'D';
    m_keymap[Key_E] = 'E';
    m_keymap[Key_F] = 'F';
    m_keymap[Key_G] = 'G';
    m_keymap[Key_H] = 'H';
    m_keymap[Key_I] = 'I';
    m_keymap[Key_J] = 'J';
    m_keymap[Key_K] = 'K';
    m_keymap[Key_L] = 'L';
    m_keymap[Key_M] = 'M';
    m_keymap[Key_N] = 'N';
    m_keymap[Key_O] = 'O';
    m_keymap[Key_P] = 'P';
    m_keymap[Key_Q] = 'Q';
    m_keymap[Key_R] = 'R';
    m_keymap[Key_S] = 'S';
    m_keymap[Key_T] = 'T';
    m_keymap[Key_U] = 'U';
    m_keymap[Key_V] = 'V';
    m_keymap[Key_W] = 'W';
    m_keymap[Key_X] = 'X';
    m_keymap[Key_Y] = 'Y';
    m_keymap[Key_Z] = 'Z';
    m_keymap[Key_0] = '0';
    m_keymap[Key_1] = '1';
    m_keymap[Key_2] = '2';
    m_keymap[Key_3] = '3';
    m_keymap[Key_4] = '4';
    m_keymap[Key_5] = '5';
    m_keymap[Key_6] = '6';
    m_keymap[Key_7] = '7';
    m_keymap[Key_8] = '8';
    m_keymap[Key_9] = '9';
    m_keymap[Key_Space] = VK_SPACE;
    m_keymap[Key_BackTick] = VK_OEM_3;
    m_keymap[Key_Minus] = VK_OEM_MINUS;
    m_keymap[Key_Equals] = VK_OEM_PLUS;
    m_keymap[Key_LeftBracket] = VK_OEM_4;
    m_keymap[Key_RightBracket] = VK_OEM_6;
    m_keymap[Key_Backslash] = VK_OEM_5;
    m_keymap[Key_Semicolon] = VK_OEM_1;
    m_keymap[Key_Apostrophe] = VK_OEM_7;
    m_keymap[Key_Comma] = VK_OEM_COMMA;
    m_keymap[Key_Period] = VK_OEM_PERIOD;
    m_keymap[Key_Slash] = VK_OEM_2;
    m_keymap[Key_Backspace] = VK_BACK;
    m_keymap[Key_Tab] = VK_TAB;
    m_keymap[Key_Enter] = VK_RETURN;
    m_keymap[Key_Escape] = VK_ESCAPE;
    m_keymap[Key_Menu] = VK_APPS;
    m_keymap[Key_LeftSystem] = VK_LWIN;
    m_keymap[Key_RightSystem] = VK_RWIN;
    m_keymap[Key_F1] = VK_F1;
    m_keymap[Key_F2] = VK_F2;
    m_keymap[Key_F3] = VK_F3;
    m_keymap[Key_F4] = VK_F4;
    m_keymap[Key_F5] = VK_F5;
    m_keymap[Key_F6] = VK_F6;
    m_keymap[Key_F7] = VK_F7;
    m_keymap[Key_F8] = VK_F8;
    m_keymap[Key_F9] = VK_F9;
    m_keymap[Key_F10] = VK_F10;
    m_keymap[Key_F11] = VK_F11;
    m_keymap[Key_F12] = VK_F12;
    m_keymap[Key_LeftShift] = VK_LSHIFT;
    m_keymap[Key_RightShift] = VK_RSHIFT;
    m_keymap[Key_LeftControl] = VK_LCONTROL;
    m_keymap[Key_RightControl] = VK_RCONTROL;
    m_keymap[Key_LeftAlt] = VK_LMENU;
    m_keymap[Key_RightAlt] = VK_RMENU;
    m_keymap[Key_Left] = VK_LEFT;
    m_keymap[Key_Right] = VK_RIGHT;
    m_keymap[Key_Up] = VK_UP;
    m_keymap[Key_Down] = VK_DOWN;
    m_keymap[Key_PageUp] = VK_PRIOR;
    m_keymap[Key_PageDown] = VK_NEXT;
    m_keymap[Key_Home] = VK_HOME;
    m_keymap[Key_End] = VK_END;
    m_keymap[Key_Insert] = VK_INSERT;
    m_keymap[Key_Delete] = VK_DELETE;
    m_keymap[Key_ScrollLock] = VK_SCROLL;
    m_keymap[Key_CapsLock] = VK_CAPITAL;
    m_keymap[Key_NumLock] = VK_NUMLOCK;
    m_keymap[Key_Num_0] = VK_NUMPAD0;
    m_keymap[Key_Num_1] = VK_NUMPAD1;
    m_keymap[Key_Num_2] = VK_NUMPAD2;
    m_keymap[Key_Num_3] = VK_NUMPAD3;
    m_keymap[Key_Num_4] = VK_NUMPAD4;
    m_keymap[Key_Num_5] = VK_NUMPAD5;
    m_keymap[Key_Num_6] = VK_NUMPAD6;
    m_keymap[Key_Num_7] = VK_NUMPAD7;
    m_keymap[Key_Num_8] = VK_NUMPAD8;
    m_keymap[Key_Num_9] = VK_NUMPAD9;
    m_keymap[Key_Num_Add] = VK_ADD;
    m_keymap[Key_Num_Subtract] = VK_SUBTRACT;
    m_keymap[Key_Num_Multiply] = VK_MULTIPLY;
    m_keymap[Key_Num_Divide] = VK_DIVIDE;
    m_keymap[Key_Num_Decimal] = VK_DECIMAL;
    m_keymap[Key_Num_Enter] = VK_RETURN; // TODO - need to handle WM_KEY* and look at lparam to distinguish from other enter key

    // Initialise frame buffer
    m_framebuffer = new Pixel[screen_width * screen_height];

    // Set default palette
    static uint32_t default_palette[256] = {
        0x000000, 0x0000a8, 0x00a800, 0x00a8a8, 0xa80000, 0xa800a8, 0xa85400, 0xa8a8a8, 0x545454, 0x5454fc, 0x54fc54, 0x54fcfc, 0xfc5454, 0xfc54fc,
        0xfcfc54, 0xfcfcfc, 0x000000, 0x141414, 0x202020, 0x2c2c2c, 0x383838, 0x444444, 0x505050, 0x606060, 0x707070, 0x808080, 0x909090, 0xa0a0a0,
        0xb4b4b4, 0xc8c8c8, 0xe0e0e0, 0xfcfcfc, 0x0000fc, 0x4000fc, 0x7c00fc, 0xbc00fc, 0xfc00fc, 0xfc00bc, 0xfc007c, 0xfc0040, 0xfc0000, 0xfc4000,
        0xfc7c00, 0xfcbc00, 0xfcfc00, 0xbcfc00, 0x7cfc00, 0x40fc00, 0x00fc00, 0x00fc40, 0x00fc7c, 0x00fcbc, 0x00fcfc, 0x00bcfc, 0x007cfc, 0x0040fc,
        0x7c7cfc, 0x9c7cfc, 0xbc7cfc, 0xdc7cfc, 0xfc7cfc, 0xfc7cdc, 0xfc7cbc, 0xfc7c9c, 0xfc7c7c, 0xfc9c7c, 0xfcbc7c, 0xfcdc7c, 0xfcfc7c, 0xdcfc7c,
        0xbcfc7c, 0x9cfc7c, 0x7cfc7c, 0x7cfc9c, 0x7cfcbc, 0x7cfcdc, 0x7cfcfc, 0x7cdcfc, 0x7cbcfc, 0x7c9cfc, 0xb4b4fc, 0xc4b4fc, 0xd8b4fc, 0xe8b4fc,
        0xfcb4fc, 0xfcb4e8, 0xfcb4d8, 0xfcb4c4, 0xfcb4b4, 0xfcc4b4, 0xfcd8b4, 0xfce8b4, 0xfcfcb4, 0xe8fcb4, 0xd8fcb4, 0xc4fcb4, 0xb4fcb4, 0xb4fcc4,
        0xb4fcd8, 0xb4fce8, 0xb4fcfc, 0xb4e8fc, 0xb4d8fc, 0xb4c4fc, 0x000070, 0x1c0070, 0x380070, 0x540070, 0x700070, 0x700054, 0x700038, 0x70001c,
        0x700000, 0x701c00, 0x703800, 0x705400, 0x707000, 0x547000, 0x387000, 0x1c7000, 0x007000, 0x00701c, 0x007038, 0x007054, 0x007070, 0x005470,
        0x003870, 0x001c70, 0x383870, 0x443870, 0x543870, 0x603870, 0x703870, 0x703860, 0x703854, 0x703844, 0x703838, 0x704438, 0x705438, 0x706038,
        0x707038, 0x607038, 0x547038, 0x447038, 0x387038, 0x387044, 0x387054, 0x387060, 0x387070, 0x386070, 0x385470, 0x384470, 0x505070, 0x585070,
        0x605070, 0x685070, 0x705070, 0x705068, 0x705060, 0x705058, 0x705050, 0x705850, 0x706050, 0x706850, 0x707050, 0x687050, 0x607050, 0x587050,
        0x507050, 0x507058, 0x507060, 0x507068, 0x507070, 0x506870, 0x506070, 0x505870, 0x000040, 0x100040, 0x200040, 0x300040, 0x400040, 0x400030,
        0x400020, 0x400010, 0x400000, 0x401000, 0x402000, 0x403000, 0x404000, 0x304000, 0x204000, 0x104000, 0x004000, 0x004010, 0x004020, 0x004030,
        0x004040, 0x003040, 0x002040, 0x001040, 0x202040, 0x282040, 0x302040, 0x382040, 0x402040, 0x402038, 0x402030, 0x402028, 0x402020, 0x402820,
        0x403020, 0x403820, 0x404020, 0x384020, 0x304020, 0x284020, 0x204020, 0x204028, 0x204030, 0x204038, 0x204040, 0x203840, 0x203040, 0x202840,
        0x2c2c40, 0x302c40, 0x342c40, 0x3c2c40, 0x402c40, 0x402c3c, 0x402c34, 0x402c30, 0x402c2c, 0x40302c, 0x40342c, 0x403c2c, 0x40402c, 0x3c402c,
        0x34402c, 0x30402c, 0x2c402c, 0x2c4030, 0x2c4034, 0x2c403c, 0x2c4040, 0x2c3c40, 0x2c3440, 0x2c3040, 0x000000, 0x000000, 0x000000, 0x000000,
        0x000000, 0x000000, 0x000000, 0x000000
    };

    set_palette(default_palette);

    if (!_opengl.init(m_hwnd))
    {
        return false;
    }

    if (!_opengl.make_current(true))
    {
        return false;
    }

    GLint status;
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &s_shader_source[0], nullptr);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetShaderInfoLog(vertex_shader, log_length, nullptr, &log[0]);
        logf("Vertex shader failed to compile:\n%s\n", log.c_str());
        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &s_shader_source[1], nullptr);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetShaderInfoLog(fragment_shader, log_length, nullptr, &log[0]);
        logf("Fragment shader failed to compile:\n%s\n", log.c_str());
        return false;
    }

    _shader_program = glCreateProgram();
    glAttachShader(_shader_program, vertex_shader);
    glAttachShader(_shader_program, fragment_shader);
    glLinkProgram(_shader_program);
    glGetProgramiv(_shader_program, GL_LINK_STATUS, &status);

    if (!status)
    {
        GLint log_length;
        glGetProgramiv(_shader_program, GL_INFO_LOG_LENGTH, &log_length);
        std::string log;
        log.resize(log_length);
        glGetProgramInfoLog(_shader_program, log_length, nullptr, &log[0]);
        logf("Program failed to link:\n%s\n", log.c_str());
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGenBuffers(1, &_vbo);
    glGenVertexArrays(1, &_vao);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);

    static const float verts[] = { -1.0f, 1.0f, -1.0f, -3.0f, 3.0f, 1.0f };
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenTextures(1, &_texture);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    _opengl.make_current(false);

    return true;
}


void App::run()
{
    _quit = false;
    _active = true;
    std::thread game_thread = std::thread(&App::engine_loop, this);
    pump_messages();
    game_thread.join();
    shutdown();
}


const App::KeyState& App::key_state(Key key)
{
    return m_keys[key];
}


const App::MouseState& App::mouse_state()
{
    return m_mouse;
}


// TODO: Repeat events - wire directly to windows keyboard events instead of synthesizing these from key states
void App::process_key_events(KeyEventHandler handler)
{
    static const std::unordered_map<Key, std::pair<char, char>> ascii_map{
        { Key_BackTick, { '`', '~' } },     { Key_Minus, { '-', '_' } },      { Key_Equals, { '=', '+' } },    { Key_LeftBracket, { '[', '{' } },
        { Key_RightBracket, { ']', '}' } }, { Key_Backslash, { '\\', '|' } }, { Key_Semicolon, { ';', ':' } }, { Key_Apostrophe, { '\'', '"' } },
        { Key_Comma, { ',', '<' } },        { Key_Period, { '.', '>' } },     { Key_Slash, { '/', '?' } }
    };

    for (int i = 0; i < Key_Count; ++i)
    {
        if (m_keys[i].pressed)
        {
            KeyEvent keypress{};
            keypress.event = Pressed;
            keypress.key = Key(i);

            if (!m_keys[Key_RightAlt].down && !m_keys[Key_LeftAlt].down)
            {
                bool shift = (m_keys[Key_LeftShift].down || m_keys[Key_RightShift].down);

                if (i >= Key_A && i <= Key_Z)
                {
                    keypress.ascii_code = (i - Key_A) + (shift ? 'A' : 'a');
                }
                else if (i >= Key_0 && i <= Key_9)
                {
                    if (shift)
                    {
                        keypress.ascii_code = ")!@#$%^&*("[i - Key_0];
                    }
                    else
                    {
                        keypress.ascii_code = '0' + (i - Key_0);
                    }
                }
                else if (i == Key_Space)
                {
                    keypress.ascii_code = ' ';
                }
                else
                {
                    const auto& lookup = ascii_map.find(Key(i));

                    if (lookup != ascii_map.end())
                    {
                        keypress.ascii_code = shift ? lookup->second.second : lookup->second.first;
                    }
                }
            }

            handler(keypress);
        }
        else if (m_keys[i].released)
        {
            KeyEvent keypress{};
            keypress.event = Released;
            keypress.key = Key(i);
        }
    }
} // namespace gli

int App::screen_width()
{
    return m_screen_width;
}


int App::screen_height()
{
    return m_screen_height;
}

void App::set_palette(uint32_t rgbx[256])
{
    for (int p = 0; p < 256; ++p)
    {
        m_palette[p] = 0xFF000000 | (rgbx[p] >> 8);
    }
}


void App::set_palette(const uint8_t* palette, int size)
{
    Pixel* dest = m_palette;

    for (int p = 0; p < size; p += 3)
    {
        dest->r = palette[p];
        dest->g = palette[p + 1];
        dest->b = palette[p + 2];
        dest->a = 0xFF;
        dest++;
    }
}


void App::load_palette(const std::string& path)
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


void App::clear_screen(uint8_t c)
{
    clear_screen(m_palette[c]);
}


void App::set_pixel(int x, int y, uint8_t p)
{
    set_pixel(x, y, m_palette[p]);
}


void App::clear_screen(Pixel p)
{
    Pixel* pixel = m_framebuffer;

    for (int y = 0; y < m_screen_height; ++y)
    {
        for (int x = 0; x < m_screen_width; ++x)
        {
            *pixel++ = p;
        }
    }
}


void App::set_pixel(int x, int y, Pixel p)
{
    if (x >= 0 && x < m_screen_width && y >= 0 && y < m_screen_height)
    {
        m_framebuffer[(y * m_screen_width) + x] = p;
    }
}


void App::draw_line(int x1, int y1, int x2, int y2, uint8_t c)
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


void App::draw_char(int x, int y, char c, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg)
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


void App::draw_string(int x, int y, const char* str, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg)
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


void App::format_string(int x, int y, const int* glyphs, int w, int h, uint8_t fg, uint8_t bg, const char* fmt, ...)
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


void App::draw_rect(int x, int y, int w, int h, uint8_t c)
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


void App::fill_rect(int x, int y, int w, int h, int bw, uint8_t fg, uint8_t bg)
{
    Pixel pfg = 0xFF000000 | *(uint32_t*)&m_palette[fg];
    Pixel pbg = 0xFF000000 | *(uint32_t*)&m_palette[bg];
    fill_rect(x, y, w, h, bw, pfg, pbg);
}


void App::fill_rect(int x, int y, int w, int h, int bw, Pixel fg, Pixel bg)
{
    Pixel colors[2] = { fg, bg };

    for (int py = 0; py < h; ++py)
    {
        for (int px = 0; px < w; ++px)
        {
            int color_index = (px < bw || px > w - 1 - bw || py < bw || py > h - 1 - bw);
            set_pixel(x + px, y + py, colors[color_index]);
        }
    }
}


void App::copy_rect(int x, int y, int w, int h, const uint8_t* src, uint32_t stride)
{
#if 0 // UNTESTED
    if (y < 0)
    {
        src += stride * -y;
        h += y;
        y = 0;
    }

    if (x < 0)
    {
        src += x;
        w += x;
        x = 0;
    }

    w = (x + w < screen_width) ? w : (screen_width - x);
    h = (y + h < screen_height) ? h : (screen_height - y);

    if (w <= 0 || h <= 0)
    {
        return;
    }

    uint8_t* dest = m_framebuffer + (x + y * screen_width) * 3;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            uint8_t p = src[x];
            RGBQUAD color = m_palette[p];
            dest[x * 3 + 0] = color.rgbRed;
            dest[x * 3 + 1] = color.rgbGreen;
            dest[x * 3 + 2] = color.rgbBlue;
        }

        dest += screen_width * 3;
        src += stride;
    }
#endif
}

void App::copy_rect_scaled(int x, int y, int w, int h, const uint8_t* src, uint32_t stride, int pixel_scale) {}


void App::draw_sprite(int x, int y, const Sprite* sprite)
{
    draw_partial_sprite(x, y, sprite, 0, 0, sprite->width(), sprite->height());
}


void App::draw_partial_sprite(int x, int y, const Sprite* sprite, int ox, int oy, int w, int h)
{
    if (x + w < 0 || y + w < 0 || x >= m_screen_width || h >= m_screen_height)
    {
        return;
    }

    if (x < 0)
    {
        ox -= x;
        w += x;
        x = 0;
    }

    if (y < 0)
    {
        oy -= y;
        h += y;
        y = 0;
    }

    if (x > m_screen_width - w)
    {
        w = m_screen_width - x;
    }

    if (y > m_screen_height - h)
    {
        h = m_screen_height - y;
    }

    if (w >= 0 && h >= 0)
    {
        Pixel* src = sprite->pixels() + ox + (oy * sprite->width());
        Pixel* dest = m_framebuffer + x + (y * m_screen_width);

        while (h--)
        {
            memcpy(dest, src, sizeof(Pixel) * w);
            src += sprite->width();
            dest += m_screen_width;
        }
    }
}


void App::shutdown()
{
    glDeleteTextures(1, &_texture);
    glDeleteVertexArrays(1, &_vao);
    glDeleteBuffers(1, &_vbo);
    glDeleteProgram(_shader_program);

    delete[] m_framebuffer;

    for (int i = 0; i < 2; ++i)
    {
        delete[] m_keystate[i];
    }

    UnregisterClass(L"IneptEngineWindow", GetModuleHandle(NULL));

    free(m_title);
}


void App::pump_messages()
{
    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}


void App::engine_loop()
{
    if (!on_create())
    {
        _quit = true;
    }

    ShowWindow(m_hwnd, SW_SHOW);

    auto prev_time = std::chrono::system_clock::now();

    while (!_quit)
    {
        auto current_time = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed_time = current_time - prev_time;
        prev_time = current_time;
        float delta = elapsed_time.count();

        wchar_t title[256];
        swprintf(title, 256, L"%s - %llu us (%0.2f fps)", m_title, uint64_t(delta * 1000000.0), 1.0f / delta);
        SetWindowText(m_hwnd, title);

        if (_active)
        {
            short* current_keystate = m_keystate[m_current_keystate];
            short* prev_keystate = m_keystate[m_current_keystate ^ 1];

            for (int i = 0; i < Key_Count; i++)
            {
                current_keystate[i] = GetAsyncKeyState(m_keymap[i]);

                m_keys[i].pressed = false;
                m_keys[i].released = false;

                if (current_keystate[i] != prev_keystate[i])
                {
                    if (current_keystate[i] & 0x8000)
                    {
                        m_keys[i].pressed = !m_keys[i].down;
                        m_keys[i].down = true;
                    }
                    else
                    {
                        m_keys[i].released = true;
                        m_keys[i].down = false;
                    }
                }
            }

            m_current_keystate ^= 1;
        }

        MouseState new_mouse_state = _mouse_state.load();

        if (!_mouse_active)
        {
            m_mouse.x = 0;
            m_mouse.y = 0;
        }
        else
        {
            m_mouse.x = new_mouse_state.x;
            m_mouse.y = new_mouse_state.y;

            static const int VkMouseButtons[3] = { VK_LBUTTON, VK_MBUTTON, VK_RBUTTON };

            for (int i = 0; i < 3; ++i)
            {
                short state = GetAsyncKeyState(VkMouseButtons[i]);
                bool was_down = m_mouse.buttons[i].down;
                bool is_down = state & 0x8000;
                m_mouse.buttons[i].down = is_down;
                m_mouse.buttons[i].pressed = is_down && !was_down;
                m_mouse.buttons[i].released = was_down && !is_down;
            }
        }

        // User update
        if (!on_update(delta))
        {
            _quit = true;
        }

        // Present
        _opengl.begin_frame();
        glBindTexture(GL_TEXTURE_2D, _texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screen_width, m_screen_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, m_framebuffer);
        glUseProgram(_shader_program);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        _opengl.end_frame();
    }

    on_destroy();

    PostMessage(m_hwnd, WM_DESTROY, 0, 0);
}


LRESULT App::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    App* app = (App*)GetProp(hwnd, L"IneptEngineWindow");

    switch (msg)
    {
        case WM_SYSKEYDOWN:
        {
            if (wparam == VK_F10)
            {
                return 0;
            }
            break;
        }
        case WM_ACTIVATEAPP:
        {
            _active = !!wparam;
            break;
        }
        case WM_CLOSE:
        {
            _quit = true;
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        case WM_MOUSELEAVE:
        {
            _mouse_active = false;
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            MouseState state = _mouse_state.load();
            int window_scale = app->m_window_width / app->m_screen_width;
            state.x = int(lparam & 0xFFFF) / window_scale;
            state.y = int(lparam >> 16) / window_scale;
            _mouse_state.store(state);
            _mouse_active = true;
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}


char* wchar_to_utf8(const wchar_t* mbcs)
{
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, mbcs, -1, NULL, 0, NULL, NULL);
    char* utf8 = (char*)malloc(utf8_len);
    WideCharToMultiByte(CP_UTF8, 0, mbcs, -1, utf8, utf8_len + 1, NULL, NULL);
    return utf8;
}


wchar_t* utf8_to_wchar(const char* utf8)
{
    int mbcs_len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t* mbcs = (wchar_t*)malloc(sizeof(wchar_t) * mbcs_len);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, mbcs, mbcs_len);
    return mbcs;
}

} // namespace gli


int wWinMainInternal(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::vector<char*> argv(__argc);

    for (int i = 0; i < __argc; ++i)
    {
        argv[i] = gli::wchar_to_utf8(__wargv[i]);
    }

    int result = gli_main(__argc, argv.data());

    for (char*& argptr : argv)
    {
        free(argptr);
    }

    CoUninitialize();

    return 0;
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    int result;

    if (IsDebuggerPresent())
    {
        result = wWinMainInternal(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
    }
    else
    {
        __try
        {
            result = wWinMainInternal(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
        }
        __except (UnhandledExceptionFilter(GetExceptionInformation()))
        {
            MessageBoxW(0, L"A wild exception appears! The application will die horribly now.", L"Oh no!", MB_ICONERROR | MB_OK);
            result = -1;
        }
    }

    return result;
}
