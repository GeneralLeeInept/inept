#include "gli_sprite.h"

#include <stb/stb_image.h>

namespace gli
{

Sprite::Sprite(int w, int h)
    : m_width(w)
    , m_height(h)
{
    m_pixels = new Pixel[w * h];
}


Sprite::~Sprite()
{
    delete[] m_pixels;
}


bool Sprite::load(const std::string& path)
{
    m_width = 0;
    m_height = 0;
    delete[] m_pixels;
    m_pixels = nullptr;

    int x = 0;
    int y = 0;
    int comp;
    stbi_uc* data = stbi_load(path.c_str(), &x, &y, &comp, 4);

    if (!data)
    {
        m_width = 0;
        m_height = 0;
        return false;
    }

    m_width = x;
    m_height = y;
    m_pixels = new Pixel[m_width * m_height];
    Pixel *p = m_pixels;

    for (y = 0; y < m_height; ++y)
    {
        for (x = 0; x < m_width; ++x)
        {
            p->r = *data++;
            p->g = *data++;
            p->b = *data++;
            p->a = *data++;
            p++;
        }
    }

    return true;
}


void Sprite::set_pixel(int x, int y, Pixel p)
{
    if (x >= 0 && x < m_width && y >= 0 && y < m_height)
    {
        m_pixels[x + y * m_width] = p;
    }
}


int Sprite::width() const
{
    return m_width;
}


int Sprite::height() const
{
    return m_height;
}


Pixel* Sprite::pixels() const
{
    return m_pixels;
}

} // namespace gli
