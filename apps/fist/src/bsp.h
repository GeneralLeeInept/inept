#pragma once

// FIST map/bsp types - totally DOOM's BSP. Reimplemented in floating point (aka lazy man's fixed point) for fun.

#include "types.h"

namespace fist
{

static const uint32_t SubSectorBit = (1u << 31);

// Defs
struct LineDef
{
    uint32_t from;
    uint32_t to;
    uint32_t flags;       // 1: 2-sided
    uint32_t sidedefs[2]; // 0: right - required
                          // 1: left  - optional ((uint32_t)-1 for one sided lines)
};

struct SideDef
{
    uint64_t textures[3]; // DOOM 8 character name stored as an 8CC. 0: mid, 1: upper, 2: lower
    uint32_t sector;
    uint32_t pad;
};

struct Sector
{
    uint64_t floor_texture; // DOOM 8 character name stored as an 8CC.
    uint64_t ceiling_texture; // DOOM 8 character name stored as an 8CC.
    float floor_height;
    float ceiling_height;
    float light_level;
};

// BSP artefacts
struct LineSeg
{
    uint32_t line_def;
    uint32_t from;
    uint32_t to;
    uint32_t side;
};

struct SubSector
{
    uint32_t sector;
    uint32_t first_seg;
    uint32_t num_segs;
};

struct Node
{
    V2f split_normal;
    float split_distance;
    BoundingBox bounds[2];
    uint32_t child[2]; // (uint32_t)-1 for none / bit 31 set if child is a sub-sector
};

struct Map
{
    ~Map();

    uint32_t num_vertices{};
    V2f* vertices{};

    uint32_t num_linedefs{};
    LineDef* linedefs{};

    uint32_t num_sidedefs{};
    SideDef* sidedefs{};

    uint32_t num_sectors{};
    Sector* sectors{};

    uint32_t num_linesegs{};
    LineSeg* linesegs{};

    uint32_t num_subsectors{};
    SubSector* subsectors{};

    uint32_t num_nodes{};
    Node* nodes{};
};

} // namespace fist
