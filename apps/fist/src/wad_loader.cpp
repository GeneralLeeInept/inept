#include "wad_loader.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace fist
{

struct WadHeader
{
    uint32_t fourcc;
    uint32_t num_lumps;
    uint32_t directory;
};

struct WadDirEntry
{
    uint32_t offs;
    uint32_t size;
    char name[8];
};

struct WadFile
{
    WadHeader* header;
    WadDirEntry* directory;
    std::vector<char> data;
};

static constexpr uint32_t make_magic(const char* id)
{
    uint32_t magic = 0;
    uint8_t pos = 0;

    while (*id && pos < 4)
    {
        magic |= *id++ << (pos++ * 8);
    }

    return magic;
}

static constexpr uint32_t magicIWAD()
{
    return make_magic("IWAD");
}

static constexpr uint32_t magicPWAD()
{
    return make_magic("PWAD");
}

WadFile* wad_open(const std::string& path)
{
    std::ifstream is(path, std::ios::binary);

    if (!is)
    {
        return nullptr;
    }

    is.seekg(0, std::ios::end);
    std::streampos file_length = is.tellg();
    is.seekg(0, std::ios::beg);

    std::unique_ptr<WadFile> wadfile = std::make_unique<WadFile>();

    size_t data_size = (size_t)((std::streamoff)file_length);
    wadfile->data.resize(data_size);
    is.read(&wadfile->data[0], data_size);

    wadfile->header = (WadHeader*)&wadfile->data[0];

    if (wadfile->header->fourcc != magicIWAD() && wadfile->header->fourcc != magicPWAD())
    {
        return nullptr;
    }

    wadfile->directory = (WadDirEntry*)&wadfile->data[wadfile->header->directory];
    return wadfile.release();
}

void wad_close(WadFile* wad)
{
    delete wad;
}

void WadFileDeleter::operator()(WadFile* wad)
{
    wad_close(wad);
}

struct LumpInfo
{
    Wad::Name name;
    size_t index;
    size_t offset;
    size_t size;
};

static bool _lump_info(WadFile* wad, const Wad::Name& lumpname, size_t start_pos, LumpInfo& info)
{
    for (size_t i = start_pos; i < wad->header->num_lumps; ++i)
    {
        if (wad->directory[i].name == lumpname)
        {
            info.name = lumpname;
            info.index = i;
            info.offset = wad->directory[i].offs;
            info.size = wad->directory[i].size;
            return true;
        }
    }

    return false;
}

bool wad_load_map(WadFile* wad, const Wad::Name& mapname, Wad::Map& map)
{
    LumpInfo map_info{};

    if (!_lump_info(wad, mapname, 0, map_info))
    {
        return false;
    }

    LumpInfo things{};
    
    if (!_lump_info(wad, "THINGS", map_info.index, things))
    {
        return false;
    }

    LumpInfo linedefs{};

    if (!_lump_info(wad, "LINEDEFS", map_info.index, linedefs))
    {
        return false;
    }

    LumpInfo vertexes{};

    if (!_lump_info(wad, "VERTEXES", map_info.index, vertexes))
    {
        return false;
    }

    size_t num_things = things.size / sizeof(Wad::ThingDef);
    map.things.resize(num_things);
    memcpy(&map.things[0], &wad->data[things.offset], num_things * sizeof(Wad::ThingDef));

    size_t num_linedefs = linedefs.size / sizeof(Wad::LineDef);
    map.linedefs.resize(num_linedefs);
    memcpy(&map.linedefs[0], &wad->data[linedefs.offset], num_linedefs * sizeof(Wad::LineDef));

    size_t num_vertexes = vertexes.size / sizeof(Wad::Vertex);
    map.vertices.resize(num_vertexes);
    memcpy(&map.vertices[0], &wad->data[vertexes.offset], num_vertexes * sizeof(Wad::Vertex));

    return true;
}

} // namespace fist
