#include "gli_opengl.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dwmapi.h>

#include "opengl/glad.h"
#include "wglext.h"

#include "gli_log.h"

struct Wgl
{
    typedef HGLRC(__stdcall* PFNWGLCREATECONTEXT)(HDC);
    typedef int(__stdcall* PFNWGLDELETECONTEXT)(HGLRC);
    typedef HGLRC(__stdcall* PFNWGLGETCURRENTCONTEXT)(void);
    typedef PROC(__stdcall* PFNWGLGETPROCADDRESS)(const char*);
    typedef int(__stdcall* PFNWGLMAKECURRENT)(HDC, HGLRC);

    HMODULE libgl = nullptr;
    PFNWGLCREATECONTEXT createContext = nullptr;
    PFNWGLDELETECONTEXT deleteContext = nullptr;
    PFNWGLGETCURRENTCONTEXT getCurrentContext = nullptr;
    PFNWGLGETPROCADDRESS getProcAddress = nullptr;
    PFNWGLMAKECURRENT makeCurrent = nullptr;
    PFNWGLSWAPINTERVALEXTPROC swapInterval = nullptr;


    bool load()
    {
        if (libgl)
        {
            gli::logm("Opengl32.dll already loaded.\n");
            return false;
        }

        libgl = LoadLibraryW(L"opengl32.dll");

        if (!libgl)
        {
            gli::logm("Failed to load Opengl32.dll.\n");
            return false;
        }

        createContext = (PFNWGLCREATECONTEXT)GetProcAddress(libgl, "wglCreateContext");
        deleteContext = (PFNWGLDELETECONTEXT)GetProcAddress(libgl, "wglDeleteContext");
        getCurrentContext = (PFNWGLGETCURRENTCONTEXT)GetProcAddress(libgl, "wglGetCurrentContext");
        getProcAddress = (PFNWGLGETPROCADDRESS)GetProcAddress(libgl, "wglGetProcAddress");
        makeCurrent = (PFNWGLMAKECURRENT)GetProcAddress(libgl, "wglMakeCurrent");

        if (!(createContext && deleteContext && getProcAddress && makeCurrent))
        {
            unload();
            return false;
        }

        return true;
    }


    void unload()
    {
        if (libgl)
        {
            FreeLibrary(libgl);
            libgl = nullptr;
        }

        createContext = nullptr;
        deleteContext = nullptr;
        getCurrentContext = nullptr;
        getProcAddress = nullptr;
        makeCurrent = nullptr;
    }
};


struct Dwm
{
    bool load()
    {
        if (dwm)
        {
            gli::logm("dwmapi.dll already loaded.\n");
            return false;
        }

        dwm = LoadLibraryW(L"dwmapi.dll");

        if (!dwm)
        {
            gli::logm("Failed to load dwmapi.dll.\n");
            return false;
        }

        flush = (PFNDWMFLUSH)GetProcAddress(dwm, "DwmFlush");

        if (!flush)
        {
            unload();
            return false;
        }

        return true;
    }


    void unload()
    {
        if (dwm)
        {
            FreeLibrary(dwm);
            dwm = nullptr;
        }

        flush = nullptr;
    }

    typedef HRESULT(__stdcall* PFNDWMFLUSH)(void);

    HMODULE dwm = nullptr;
    PFNDWMFLUSH flush = nullptr;
};


static Wgl wgl;
static Dwm dwm;


class WindowCleaner
{
public:
    WindowCleaner(HINSTANCE instance)
        : _instance(instance)
    {
    }


    ~WindowCleaner() { release(); }


    bool set_window_class(ATOM wc)
    {
        _wc = wc;
        return !!_wc;
    }


    bool set_hwnd(HWND wnd)
    {
        _wnd = wnd;
        return !!_wnd;
    }


    bool set_dc(HDC dc)
    {
        _dc = dc;
        return !!_dc;
    }


    bool set_glrc(HGLRC glrc)
    {
        _glrc = glrc;
        return !!_glrc;
    }


    void release()
    {
        if (_glrc)
        {
            if (wgl.getCurrentContext() == _glrc)
            {
                wgl.makeCurrent(nullptr, nullptr);
            }

            wgl.deleteContext(_glrc);
        }

        if (_dc)
        {
            ReleaseDC(_wnd, _dc);
        }

        if (_wnd)
        {
            DestroyWindow(_wnd);
        }

        if (_wc)
        {
            UnregisterClass(MAKEINTATOM(_wc), _instance);
        }
    }


    explicit operator HINSTANCE() { return _instance; }
    explicit operator HWND() { return _wnd; }
    explicit operator HDC() { return _dc; }
    explicit operator HGLRC() { return _glrc; }

private:
    HINSTANCE _instance;
    ATOM _wc = 0;
    HWND _wnd = nullptr;
    HDC _dc = nullptr;
    HGLRC _glrc = nullptr;
};


bool create_dummy_window(WindowCleaner& cleaner)
{
    static LPCWSTR _dummy_window_class = L"_raptor_ogl";
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = (HINSTANCE)cleaner;
    wc.lpszClassName = _dummy_window_class;

    if (!cleaner.set_window_class(RegisterClassExW(&wc)))
    {
        return false;
    }

    HWND hwnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, _dummy_window_class, _dummy_window_class, WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 1, 1,
                                nullptr, nullptr, (HINSTANCE)cleaner, nullptr);

    if (!hwnd)
    {
        return false;
    }

    cleaner.set_hwnd(hwnd);
    ShowWindow((HWND)cleaner, SW_HIDE);

    MSG msg;
    while (PeekMessageW(&msg, (HWND)cleaner, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}


bool load_gl(WindowCleaner& cleaner)
{
    if (!cleaner.set_dc(GetDC((HWND)cleaner)))
    {
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixel_format = ChoosePixelFormat((HDC)cleaner, &pfd);

    if (!pixel_format)
    {
        return false;
    }

    if (!SetPixelFormat((HDC)cleaner, pixel_format, &pfd))
    {
        return false;
    }

    HGLRC glrc = wgl.createContext((HDC)cleaner);

    if (!glrc)
    {
        gli::logf("wglCreateContext failed - %08X\n", GetLastError());
        return false;
    }
    
    cleaner.set_glrc(glrc);

    if (!wgl.makeCurrent((HDC)cleaner, (HGLRC)cleaner))
    {
        return false;
    }
    
    if (!gladLoadGL())
    {
        return false;
    }

    wgl.swapInterval = (PFNWGLSWAPINTERVALEXTPROC)wgl.getProcAddress("wglSwapIntervalEXT");

    return true;
}


namespace gli
{
bool OpenGL::init(HWND hwnd)
{
    if (!(dwm.load() && wgl.load()))
    {
        return false;
    }

    HINSTANCE hinstance = GetModuleHandle(nullptr);
    WindowCleaner cleaner(hinstance);
    
    if (!(create_dummy_window(cleaner) && load_gl(cleaner)))
    {
        return false;
    }

    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
    wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wgl.getProcAddress("wglChoosePixelFormatARB"));

    if (!wglChoosePixelFormatARB)
    {
        return false;
    }

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
    wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wgl.getProcAddress("wglCreateContextAttribsARB"));

    if (!wglCreateContextAttribsARB)
    {
        return false;
    }

    cleaner.release();

    class LocalDc
    {
    public:
        LocalDc(HWND wnd)
            : _wnd(wnd)
        {
            _dc = GetDC(wnd);
        }

        ~LocalDc()
        {
            if (_dc)
            {
                ReleaseDC(_wnd, _dc);
            }
        }

        void reset() { _dc = nullptr; }

        operator bool() { return !!_dc; }
        operator HDC() { return _dc; }

    private:
        HWND _wnd;
        HDC _dc;
    };

    LocalDc dc((HWND)hwnd);

    if (!dc)
    {
        return false;
    }

    /* clang-format off */
    const int pixel_attributes[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
        WGL_RED_BITS_ARB,       8,
        WGL_GREEN_BITS_ARB,     8,
        WGL_BLUE_BITS_ARB,      8,
        WGL_ALPHA_BITS_ARB,     8,
        WGL_DEPTH_BITS_ARB,     24,
        WGL_STENCIL_BITS_ARB,   8,
        WGL_SAMPLE_BUFFERS_ARB, 1,
        WGL_SAMPLES_ARB,        4,
        0, 0
    };
    /* clang-format on */

    int pixel_format;
    UINT num_formats;

    if (!(wglChoosePixelFormatARB(dc, pixel_attributes, nullptr, 1, &pixel_format, &num_formats) && num_formats))
    {
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd;

    if (!(DescribePixelFormat(dc, pixel_format, sizeof(pfd), &pfd) && SetPixelFormat(dc, pixel_format, &pfd)))
    {
        return false;
    }

    /* clang-format off */
    int context_attributes[] =
    { 
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4, 
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 
        0
    };
    /* clang-format on */

    HGLRC glrc = wglCreateContextAttribsARB(dc, 0, context_attributes);

    if (!glrc)
    {
        return false;
    }

    if (!wgl.makeCurrent(dc, glrc))
    {
        wgl.deleteContext(glrc);
        return false;
    }

    wgl.makeCurrent(nullptr, nullptr);

    _hwnd = hwnd;
    _dc = dc;
    _rc = glrc;

    dc.reset();

    return true;
}


bool OpenGL::make_current(bool current)
{
    return !!wgl.makeCurrent(current ? _dc : nullptr, current ? _rc : nullptr);
}


void OpenGL::begin_frame()
{
    if (!make_current(true))
    {
        return;
    }

    if (wgl.swapInterval)
    {
        wgl.swapInterval(1);
    }
}


void OpenGL::end_frame()
{
    if (!wgl.swapInterval && dwm.flush)
    {
        dwm.flush();
    }

    SwapBuffers(_dc);
    make_current(false);
}


void OpenGL::clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

}