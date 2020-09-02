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


template <typename T>
bool vread(T* output, size_t count, std::vector<uint8_t>& data, size_t& read_ptr)
{
    size_t readsize = sizeof(T) * count;

    if (readsize > data.size() - read_ptr)
    {
        return false;
    }

    memcpy(output, &data[read_ptr], readsize);
    read_ptr += readsize;
    return true;
}


template <typename T>
bool vread(T& output, std::vector<uint8_t>& data, size_t& read_ptr)
{
    return vread(&output, 1, data, read_ptr);
}


bool vread(std::string& s, std::vector<uint8_t>& data, size_t& read_ptr)
{
    uint16_t len;

    if (!vread(len, data, read_ptr))
    {
        return false;
    }

    s.resize(len);
    return vread(&s[0], len, data, read_ptr);
}


template <typename T>
bool vread(std::vector<T>& v, size_t count, std::vector<uint8_t>& data, size_t& read_ptr)
{
    v.resize(count);
    return vread(&v[0], count, data, read_ptr);
}


template <typename T>
bool vread(std::vector<T>& v, std::vector<uint8_t>& data, size_t& read_ptr)
{
    uint16_t count;

    if (!vread(count, data, read_ptr))
    {
        return false;
    }

    return vread(v, count, data, read_ptr);
}


bool TileMap::load(const std::string& path)
{
    std::vector<uint8_t> map_data;

    if (!GliFileSystem::get()->read_entire_file(path.c_str(), map_data))
    {
        return false;
    }

    size_t read_ptr = 0;
    uint32_t magic;

    if (!vread(magic, map_data, read_ptr) || magic != map4cc())
    {
        return false;
    }

    std::string tilesheet_path;
    bool success = true;

    success = success && vread(_tile_size, map_data, read_ptr);
    success = success && vread(_width, map_data, read_ptr);
    success = success && vread(_height, map_data, read_ptr);
    success = success && vread(_tiles, _width * _height, map_data, read_ptr);
    success = success && vread(_player_spawn, map_data, read_ptr);
    success = success && vread(_ai_spawns, map_data, read_ptr);
    success = success && vread(tilesheet_path, map_data, read_ptr);
    success = success && vread(_tile_info, map_data, read_ptr);
    success = success && _tilesheet.load(asset_path(tilesheet_path));

    return success;
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


const V2f& TileMap::player_spawn() const
{
    return _player_spawn;
}


const std::vector<V2f>& TileMap::ai_spawns() const
{
    return _ai_spawns;
}

} // namespace Bootstrap