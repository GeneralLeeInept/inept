#pragma once

#include "gli_core.h"   // for gli::Pixel

#include <string>

namespace gli
{

class Sprite
{
public:
    Sprite() = default;
    Sprite(int w, int h);

    bool load(const std::string& path);
    void set_pixel(int x, int y, Pixel p);

    int width();
    int height();
    Pixel* pixels();

private:
    const Pixel* pixels;
};

}
