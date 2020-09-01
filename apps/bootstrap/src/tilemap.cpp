#include "tilemap.h"

#include <gli.h>

constexpr uint32_t map4cc()
{
    uint32_t magic = 0;
    return magic | ('M' << 24) | ('A' << 16) | ('P' << 8) | ' ';
}

namespace Bootstrap
{

std::string asset_path(const std::string& path)
{
    // This is all because I messed up the filesystem - I don't want to have to special case every file path I use.
#if GLI_SHIPPING
    const std::string container = "//assets.glp//";
#else
    const std::string container;
#endif

    size_t size = std::snprintf(nullptr, 0, "%s%s", container.c_str(), path.c_str()) + 1;
    std::string buf(size, '\0');
    std::snprintf(&buf[0], size, "%s%s", container.c_str(), path.c_str());
    return buf;
}


bool TileMap::load(const std::string& path)
{
    std::vector<uint8_t> map_data;

    if (!GliFileSystem::get()->read_entire_file(path.c_str(), map_data))
    {
        return false;
    }

    uint8_t* read_ptr = &map_data[0];
    size_t bytes_remaining = map_data.size();

    if (bytes_remaining < sizeof(uint32_t))
    {
        return false;
    }

    uint32_t magic = *(uint32_t*)read_ptr;
    read_ptr += sizeof(uint32_t);
    bytes_remaining -= sizeof(uint32_t);

    if (magic != map4cc())
    {
        return false;
    }

    if (bytes_remaining < 3 * sizeof(uint16_t))
    {
        return false;
    }

    _tile_size = ((uint16_t*)read_ptr)[0];
    _width = ((uint16_t*)read_ptr)[1];
    _height = ((uint16_t*)read_ptr)[2];
    read_ptr += 3 * sizeof(uint16_t);
    bytes_remaining -= 3 * sizeof(uint16_t);

    size_t tile_data_size = sizeof(uint16_t) * _width * _height;

    if (bytes_remaining < tile_data_size)
    {
        return false;
    }

    _tiles.resize(_width * _height);
    memcpy(&_tiles[0], read_ptr, tile_data_size);
    read_ptr += tile_data_size;
    bytes_remaining -= tile_data_size;

    if (bytes_remaining < sizeof(uint16_t))
    {
        return false;
    }

    size_t tilesheet_path_len = *(uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);
    bytes_remaining -= sizeof(uint16_t);

    if (bytes_remaining < tilesheet_path_len)
    {
        return false;
    }

    std::string tilesheet_path;
    tilesheet_path.resize(tilesheet_path_len);
    memcpy(&tilesheet_path[0], read_ptr, tilesheet_path_len);
    read_ptr += tilesheet_path_len;
    bytes_remaining -= tilesheet_path_len;

    if (!_tilesheet.load(asset_path(tilesheet_path)))
    {
        return false;
    }

    if (bytes_remaining < sizeof(uint16_t))
    {
        return false;
    }

    size_t tile_count = *(uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);
    bytes_remaining -= sizeof(uint16_t);

    if (bytes_remaining < tile_count * sizeof(TileInfo))
    {
        return false;
    }

    _tile_info.resize(tile_count);
    memcpy(&_tile_info[0], read_ptr, tile_count * sizeof(TileInfo));
    read_ptr += tile_count * sizeof(TileInfo);
    bytes_remaining -= tile_count * sizeof(TileInfo);

    return true;
}


const gli::Sprite& TileMap::tilesheet() const
{
    return _tilesheet;
}


uint16_t TileMap::tile_size() const
{
    return _tile_size;
}


uint16_t TileMap::width() const
{
    return _width;
}


uint16_t TileMap::height() const
{
    return _height;
}


uint16_t TileMap::operator()(uint16_t x, uint16_t y) const
{
    uint16_t t = 0;

    if (x < _width && y < _height)
    {
        t = _tiles.at(x + y * _width);
    }

    return t;
}


bool TileMap::walkable(uint16_t x, uint16_t y) const
{
    uint16_t tile = operator()(x, y);
    bool result = false;

    if (tile)
    {
        result = _tile_info[tile - 1].walkable;
    }

    return result;
}


void TileMap::draw_info(uint16_t tile_index, int& ox, int& oy, bool& has_alpha) const
{
    int num_columns = _tilesheet.width() / _tile_size;
    int tx = tile_index % num_columns;
    int ty = tile_index / num_columns;
    ox = tx * _tile_size;
    oy = ty * _tile_size;
    has_alpha = false;
}


} // namespace Bootstrap