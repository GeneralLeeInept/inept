#include "gli_sprite.h"
#include "gli_file.h"

#include <stb/stb_image.h>
#include <vector>

namespace gli
{

Sprite::Sprite(int w, int h)
    : m_width(w)
    , m_height(h)
{
    m_pixels.reset(new Pixel[w * h]);
}


Sprite::~Sprite()
{
    m_pixels.reset();
}


bool Sprite::load(const std::string& path)
{
    unload();

    std::vector<uint8_t> image_data;

    if (!GliFileSystem::get()->read_entire_file(path.c_str(), image_data))
    {
        return false;
    }

    int x = 0;
    int y = 0;
    int comp;
    stbi_uc* data = stbi_load_from_memory((const stbi_uc*)image_data.data(), (int)image_data.size(), &x, &y, &comp, 4);

    if (!data)
    {
        m_width = 0;
        m_height = 0;
        return false;
    }

    m_width = x;
    m_height = y;
    m_pixels.reset(new Pixel[m_width * m_height]);
    Pixel *p = m_pixels.get();

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


void Sprite::unload()
{
    m_width = 0;
    m_height = 0;
    m_pixels.reset();
}


void Sprite::set_pixel(int x, int y, Pixel p)
{
    if (x >= 0 && x < m_width && y >= 0 && y < m_height)
    {
        m_pixels.get()[x + y * m_width] = p;
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
    return m_pixels.get();
}

} // namespace gli
