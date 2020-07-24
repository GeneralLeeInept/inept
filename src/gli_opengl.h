#pragma once

#include <cstdint>

typedef struct HDC__* HDC;
typedef struct HGLRC__* HGLRC;
typedef struct HWND__* HWND;

namespace gli
{

class OpenGL
{
public:
    struct Tex2D;

    bool init(HWND hwnd);
    bool make_current(bool current);

    Tex2D* create_tex2D(uint32_t width, uint32_t height);

    void begin_frame();
    void end_frame();

    void clear(float r, float g, float b, float a);

private:
    HWND _hwnd;
    HDC _dc;
    HGLRC _rc;
};

}
