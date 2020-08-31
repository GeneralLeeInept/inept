#pragma once

#include <gli.h>

#include <string>
#include <vector>

namespace Bootstrap
{

class TileMap
{
public:
    bool load(const std::string& path);

    const gli::Sprite& tilesheet() const;
    uint16_t tile_size() const;
    uint16_t width() const;
    uint16_t height() const;
    uint16_t operator()(uint16_t x, uint16_t y) const;

    void tile_info(uint16_t tile_index, int& ox, int& oy, bool& has_alpha);

private:
    std::vector<uint16_t> _tiles;
    gli::Sprite _tilesheet;
    uint16_t _tile_size;
    uint16_t _width;
    uint16_t _height;
};

} // namespace Bootstrap
