#include "tilemap.h"

#include "assets.h"
#include "collision.h"
#include "vread.h"

#include <gli.h>

namespace Bootstrap
{

constexpr uint32_t map4cc()
{
    uint32_t magic = 0;
    return magic | ('M' << 24) | ('A' << 16) | ('P' << 8) | ' ';
}


template <>
bool vread(TileMap::Zone& z, std::vector<uint8_t>& data, size_t& read_ptr)
{
    bool result = true;
    result = result && vread(z.rect, data, read_ptr);
    result = result && vread(z.name, data, read_ptr);
    return result;
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
    success = success && vread(_min_tile_x, map_data, read_ptr);
    success = success && vread(_min_tile_y, map_data, read_ptr);
    success = success && vread(_max_tile_x, map_data, read_ptr);
    success = success && vread(_max_tile_y, map_data, read_ptr);
    success = success && vread(_player_spawn, map_data, read_ptr);
    success = success && vread(_ai_spawns, map_data, read_ptr);
    success = success && vread(_zones, map_data, read_ptr);
    success = success && vread(tilesheet_path, map_data, read_ptr);
    success = success && vread(_tile_info, map_data, read_ptr);
    success = success && _tilesheet.load(asset_path(tilesheet_path));

    return success;
}


bool TileMap::load_minimap(const std::string& path)
{
    return _minimap.load(path);
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


uint8_t TileMap::tile_flags(uint16_t x, uint16_t y) const
{
    uint16_t tile = operator()(x, y);
    uint8_t flags = TileFlag::BlocksLos;

    if (tile)
    {
        flags = _tile_info[tile - 1].flags;
    }

    return flags;
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


bool TileMap::raycast(const V2f& from, const V2f& to, uint8_t filter_flags, V2f* hit_out) const
{
    // Cast ray through the map and find the nearest intersection with a tile matching the filter
    V2f r = to - from;
    V2f closest_hit = to;

    float distance = FLT_MAX;

    if (r.x > 0.0f)
    {
        float dydx = r.y / r.x;
        int tx = (int)(std::floor(from.x + 1.0f));

        for (; tx < _width; ++tx)
        {
            float y = from.y + dydx * (tx - from.x);
            int ty = (int)std::floor(y);

            if (ty < 0 || ty >= _height)
            {
                break;
            }

            if ((tile_flags(tx, ty) & filter_flags) != 0)
            {
                V2f hit{ (float)tx, (float)y };
                float hit_d = length_sq(hit - from);

                if (hit_d < distance)
                {
                    distance = hit_d;
                    closest_hit = hit;
                }

                break;
            }
        }
    }
    else if (r.x < 0.0f)
    {
        float dydx = r.y / r.x;
        int tx = (int)std::floor(from.x - 1.0f);

        for (; tx >= 0; tx--)
        {
            float y = from.y + dydx * (tx + 1.0f - from.x);
            int ty = (int)std::floor(y);

            if (ty < 0 || ty >= _height)
            {
                break;
            }

            if ((tile_flags(tx, ty) & filter_flags) != 0)
            {
                V2f hit{ (float)(tx + 1), (float)y };
                float hit_d = length_sq(hit - from);

                if (hit_d < distance)
                {
                    distance = hit_d;
                    closest_hit = hit;
                }

                break;
            }
        }
    }

    if (r.y > 0.0f)
    {
        float dxdy = r.x / r.y;
        int ty = (int)std::floor(from.y + 1.0f);

        for (; ty < _height; ty++)
        {
            float x = from.x + dxdy * (ty - from.y);
            int tx = (int)std::floor(x);

            if (tx < 0 || tx >= _width)
            {
                break;
            }

            if ((tile_flags(tx, ty) & filter_flags) != 0)
            {
                V2f hit{ (float)x, (float)ty };
                float hit_d = length_sq(hit - from);

                if (hit_d < distance)
                {
                    distance = hit_d;
                    closest_hit = hit;
                }

                break;
            }
        }
    }
    else if (r.y < 0.0f)
    {
        float dxdy = r.x / r.y;
        int ty = (int)std::floor(from.y - 1.0f);

        for (; ty >= 0; ty--)
        {
            float x = from.x + dxdy * (ty + 1.0f - from.y);
            int tx = (int)std::floor(x);

            if (tx < 0 || tx >= _width)
            {
                break;
            }

            if ((tile_flags(tx, ty) & filter_flags) != 0)
            {
                V2f hit{ (float)x, (float)(ty + 1) };
                float hit_d = length_sq(hit - from);

                if (hit_d < distance)
                {
                    distance = hit_d;
                    closest_hit = hit;
                }

                break;
            }
        }
    }

    if (hit_out)
    {
        *hit_out = closest_hit;
    }

    return distance <= length_sq(r);
}


const TileMap::Zone* TileMap::get_zone(const V2f& p, const Zone* hint) const
{
    if (hint && contains(hint->rect, p))
    {
        return hint;
    }

    for (const Zone& zone : _zones)
    {
        if (contains(zone.rect, p))
        {
            return &zone;
        }
    }

    return nullptr;
}


const gli::Sprite& TileMap::minimap() const
{
    return _minimap;
}

void TileMap::pos_to_minimap(const V2f& p, int& x, int& y) const
{
    V2f mins = V2f{ (float)_min_tile_x, (float)_min_tile_y };
    V2f maxs = V2f{ (float)_max_tile_x, (float)_max_tile_y };

    if (p.x < mins.x || p.x >= maxs.x || p.y < mins.y || p.y >= maxs.y)
    {
        // Not on the minimap(?!)
        x = -1;
        y = -1;
    }
    else
    {
        V2f extent = maxs - mins;
        V2f minimap_scale = V2f{ (float)_minimap.width(), (float)_minimap.height() } / extent;
        V2f minimap_pos = V2f{p - mins} * minimap_scale;
        x = (int)(minimap_pos.x + 0.5f);
        y = (int)(minimap_pos.y + 0.5f);
    }
}

} // namespace Bootstrap