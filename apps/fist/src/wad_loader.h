#pragma once

#include <string>
#include <vector>

#include "types.h"

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

    union
    {
        char c[8]{};
        uint32_t i[2];
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
    return (left.i[0] == right.i[0] && left.i[1] == right.i[1]);
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
    Name texmid;
    Name texlower;
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
} // namespace Wad

WadFile* wad_open(const std::string& path);
void wad_close(WadFile* wad);

bool wad_load_map(WadFile* wad, const Wad::Name& mapname, Wad::Map& map);

} // namespace fist
