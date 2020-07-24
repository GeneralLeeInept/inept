#include "gli_core.h"

#include "gli_log.h"
#include "gli_opengl.h"

#include "opengl40/glad.h"

#include <objbase.h>
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
uint8_t* _texture_data = nullptr;

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

    for (int i = 0; i < 2; ++i)
    {
        m_framebuffer[i] = new uint8_t[buffer_size];
        memset(m_framebuffer[i], 0, buffer_size);
    }

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

    _texture_data = new uint8_t[screen_width * screen_height * 3];

    return true;
}


void App::run()
{
    std::thread game_thread = std::thread(&App::engine_loop, this);
    pump_messages();
    game_thread.join();
    shutdown();
}


void App::shutdown()
{
    delete[] _texture_data;
    glDeleteTextures(1, &_texture);
    glDeleteVertexArrays(1, &_vao);
    glDeleteBuffers(1, &_vbo);
    glDeleteProgram(_shader_program);

    for (int i = 0; i < 2; ++i)
    {
        delete[] m_framebuffer[i];
    }

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
        PostQuitMessage(0);
        return;
    }

    ShowWindow(m_hwnd, SW_SHOW);

    bool active = true;

    auto prev_time = std::chrono::system_clock::now();

    while (active)
    {
        auto current_time = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed_time = current_time - prev_time;
        prev_time = current_time;
        float delta = elapsed_time.count();

        wchar_t title[256];
        swprintf(title, 256, L"%s - %llu us (%0.2f fps)", m_title, uint64_t(delta * 1000000.0), 1.0f / delta);
        SetWindowText(m_hwnd, title);

        // User update
        if (!on_update(delta))
        {
            active = false;
        }

        // Present
        _opengl.begin_frame();
        glBindTexture(GL_TEXTURE_2D, _texture);
        uint8_t* texdataptr = _texture_data;

        for (int y = 0; y < screen_height; ++y)
        {
            for (int x = 0; x < screen_width; ++x)
            {
                uint8_t src = m_framebuffer[m_frontbuffer ^ 1][y * screen_width + x];
                RGBQUAD color = m_palette[src];
                *texdataptr++ = color.rgbRed;
                *texdataptr++ = color.rgbGreen;
                *texdataptr++ = color.rgbBlue;
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_width, screen_height, 0, GL_RGB, GL_UNSIGNED_BYTE, _texture_data);
        glUseProgram(_shader_program);
        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        _opengl.end_frame();

        // Check if Window closed
        active = active && !!IsWindow(m_hwnd);
    }

    on_destroy();
}


LRESULT App::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    App* app = (App*)GetProp(hwnd, L"IneptEngineWindow");

    switch (msg)
    {
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


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
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
