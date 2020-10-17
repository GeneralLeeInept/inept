#pragma once

#include "types.h"

#include <gli.h>

#include <string>
#include <vector>

namespace fist
{

struct WadFile;

struct WadFileDeleter
{
    void operator()(WadFile*);
};

namespace Wad
{
struct Name
{
    Name() = default;
    Name(const char* str) { *this = str; }
    Name(uint64_t i64) { this->i64 = i64; }

    union
    {
        char c[8]{};
        uint32_t i[2];
        uint64_t i64;
    };

    Name& operator=(const char* str)
    {
        if (str)
        {
            std::strncpy(c, str, 8);
        }

        return *this;
    }
};

inline bool operator==(const Name& left, const Name& right)
{
    // case insensitive?!?
    //return (left.i64 == right.i64);
    for (int i = 0; i < 8; ++i)
    {
        if (std::toupper(left.c[i]) != std::toupper(right.c[i]))
        {
            return false;
        }
    }
}

inline bool operator<(const Name& left, const Name& right)
{
    // string ordering
    for (int i = 0; i < 8; ++i)
    {
        if (std::toupper(left.c[i]) != std::toupper(left.c[i]))
        {
            return std::toupper(left.c[i]) < std::toupper(left.c[i]);
        }
    }
    return false;
}

enum ThingType
{
    Player1Start = 1,
};

#pragma pack(push, 1)
struct ThingDef
{
    int16_t xpos;
    int16_t ypos;
    uint16_t angle;
    uint16_t type;
    uint16_t options;
};

struct LineDef
{
    uint16_t from;
    uint16_t to;
    uint16_t flags;
    uint16_t type;
    uint16_t tag;
    uint16_t sidedefs[2];   // right, left
};

struct SideDef
{
    int16_t xoffset;
    int16_t yoffset;
    Name texupper;
    Name texlower;
    Name texmid;
    int16_t sector;
};

struct Vertex
{
    int16_t x;
    int16_t y;
};

struct Sector
{
    int16_t floor;
    int16_t ceiling;
    Name texfloor;
    Name texceiling;
    int16_t light;
    int16_t special;
    int16_t tag;
};
#pragma pack(pop)

struct Map
{
    std::vector<ThingDef> things;
    std::vector<LineDef> linedefs;
    std::vector<SideDef> sidedefs;
    std::vector<Vertex> vertices;
    std::vector<Sector> sectors;
};

struct Texture
{
    int16_t width;
    int16_t height;
    std::vector<gli::Pixel> pixels;
};

} // namespace Wad

WadFile* wad_open(const std::string& path);
void wad_close(WadFile* wad);

bool wad_load_map(WadFile* wad, const Wad::Name& mapname, Wad::Map& map);
bool wad_load_texture(WadFile* wad, const Wad::Name& texname, Wad::Texture& texture);

using TextureLoadCallback = std::function<void(int64_t, const Wad::Texture&, void*)>;
void wad_load_all_textures(WadFile* wad, TextureLoadCallback load_callback, void* user_data);

} // namespace fist
