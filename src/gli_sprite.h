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
    ~Sprite();

    bool load(const std::string& path);
    void set_pixel(int x, int y, Pixel p);

    int width() const;
    int height() const;
    Pixel* pixels() const;

private:
    int m_width{};
    int m_height{};
    Pixel* m_pixels{};
};

}
