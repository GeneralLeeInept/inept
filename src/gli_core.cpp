#include "gli_core.h"

#include "gli_log.h"
#include "gli_opengl.h"

#include "opengl40/glad.h"

#include <objbase.h>

#include <atomic>
#include <thread>


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


bool App::initialize(const char* name, int screen_width_, int screen_height_, int window_scale)
{
    m_title = utf8_to_wchar(name);

    screen_width = screen_width_;
    screen_height = screen_height_;
    window_width = screen_width * window_scale;
    window_height = screen_height * window_scale;

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

    RECT client_rect = { 0, 0, window_width, window_height };
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

    // Initialise frame buffer
    size_t buffer_size = size_t(screen_width) * size_t(screen_height);
    m_framebuffer = new uint8_t[screen_width * screen_height * 3];

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

    if(!status)
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


void App::set_palette(uint32_t rgbx[256])
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


void App::set_palette(const uint8_t* palette, int size)
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
    RGBQUAD color = m_palette[c];
    uint8_t* pixel = m_framebuffer;

    for (int y = 0; y < screen_height; ++y)
    {
        for (int x = 0; x < screen_width; ++x)
        {
            *pixel++ = color.rgbRed;
            *pixel++ = color.rgbGreen;
            *pixel++ = color.rgbBlue;
        }
    }
}


void App::set_pixel(int x, int y, uint8_t p)
{
    RGBQUAD color = m_palette[p];
    uint8_t* pixel = &m_framebuffer[((y * screen_width) + x) * 3];
    pixel[0] = color.rgbRed;
    pixel[1] = color.rgbGreen;
    pixel[2] = color.rgbBlue;
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


void App::copy_rect(int x, int y, int w, int h, const uint8_t* src, uint32_t stride)
{
#if 0   // UNTESTED
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

void App::copy_rect_scaled(int x, int y, int w, int h, const uint8_t* src, uint32_t stride, int pixel_scale)
{
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

            for (int i = 0; i < 256; i++)
            {
                current_keystate[i] = GetAsyncKeyState(i);

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

        // User update
        if (!on_update(delta))
        {
            _quit = true;
        }

        // Present
        _opengl.begin_frame();
        glBindTexture(GL_TEXTURE_2D, _texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_width, screen_height, 0, GL_RGB, GL_UNSIGNED_BYTE, m_framebuffer);
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

            gli::logf("wParam = %04X, lParam = %04X\n", wparam, lparam);

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
