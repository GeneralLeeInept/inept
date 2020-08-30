#pragma once

#include "gli_core.h"   // for gli::Pixel

#include <memory>
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
    void unload();

    void set_pixel(int x, int y, Pixel p);

    int width() const;
    int height() const;
    Pixel* pixels() const;

private:
    int m_width{};
    int m_height{};
    std::unique_ptr<Pixel> m_pixels{};
};

}
