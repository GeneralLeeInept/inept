#pragma once

#include "types.h"

#include <gli.h>

#include <string>
#include <vector>

namespace Bootstrap
{

class TileMap
{
public:
    enum TileFlag
    {
        BlocksMovables = 1,
        BlocksLos = 2,
        BlocksBullets = 4,
        BlocksAI = 8,
    };

    struct TileInfo
    {
        uint16_t tileid;
        uint8_t flags;
    };

    struct Zone
    {
        Rect rect;
        std::string name;
    };

    bool load(const std::string& path);
    bool load_minimap(const std::string& path);

    const gli::Sprite& tilesheet() const;
    uint16_t tile_size() const;
    uint16_t width() const;
    uint16_t height() const;
    uint16_t operator()(uint16_t x, uint16_t y) const;
    uint8_t tile_flags(uint16_t x, uint16_t y) const;
    void draw_info(uint16_t tile_index, int& ox, int& oy, bool& has_alpha) const;
    const V2f& player_spawn() const;
    const std::vector<V2f>& ai_spawns() const;
    bool raycast(const V2f& from, const V2f& to, uint8_t filter_flags, V2f* hit) const;
    const Zone* get_zone(const V2f& p, const Zone* hint) const;
    const gli::Sprite& minimap() const;
    void pos_to_minimap(const V2f& p, int& x, int& y) const;

private:
    std::vector<uint16_t> _tiles;
    std::vector<TileInfo> _tile_info;
    std::vector<V2f> _ai_spawns;
    std::vector<Zone> _zones;
    gli::Sprite _tilesheet;
    gli::Sprite _minimap;
    V2f _player_spawn;
    uint16_t _tile_size;
    uint16_t _width;
    uint16_t _height;
    uint16_t _min_tile_x;
    uint16_t _min_tile_y;
    uint16_t _max_tile_x;
    uint16_t _max_tile_y;
};

} // namespace Bootstrap
