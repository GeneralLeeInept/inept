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

    LumpInfo sidedefs{};

    if (!_lump_info(wad, "SIDEDEFS", map_info.index, sidedefs))
    {
        return false;
    }

    LumpInfo sectors{};

    if (!_lump_info(wad, "SECTORS", map_info.index, sectors))
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

    size_t num_sidedefs = sidedefs.size / sizeof(Wad::SideDef);
    map.sidedefs.resize(num_sidedefs);
    memcpy(&map.sidedefs[0], &wad->data[sidedefs.offset], num_sidedefs * sizeof(Wad::SideDef));

    size_t num_sectors = sectors.size / sizeof(Wad::Sector);
    map.sectors.resize(num_sectors);
    memcpy(&map.sectors[0], &wad->data[sectors.offset], num_sectors * sizeof(Wad::Sector));

    return true;
}

bool wad_load_texture(WadFile* wad, const Wad::Name& texname, Wad::Texture& texture)
{
    #pragma pack(push,1)
    struct TexHeader
    {
        Wad::Name name;
        int16_t pad1[2];
        int16_t width;
        int16_t height;
        int16_t pad2[2];
        int16_t num_patches;
    };

    struct PatchDesc
    {
        int16_t xpos;
        int16_t ypos;
        int16_t pname;
        int16_t stepdir;
        int16_t colormap;
    };

    struct PicHeader
    {
        int16_t width;
        int16_t height;
        int16_t left_offset;
        int16_t top_offset;
    };
    #pragma pack(pop)

    LumpInfo textures[2]{};

    _lump_info(wad, "TEXTURE1", 0, textures[0]);
    _lump_info(wad, "TEXTURE2", 0, textures[1]);

    TexHeader* texptr = nullptr;
    bool found = false;

    for (int i = 0; !found && i < 2; ++i)
    {
        int* texture_offsets = ((int*)&wad->data[textures[i].offset]);
        int smin = 0;
        int smax = texture_offsets[0];

        while (smin <= smax)
        {
            int sindex = (smin + smax) / 2;
            size_t texture_offset = textures[i].offset + texture_offsets[sindex + 1];
            texptr = (TexHeader*)&wad->data[texture_offset];

            if (texptr->name == texname)
            {
                found = true;
                break;
            }
            else if (texname < texptr->name)
            {
                smax = sindex - 1;
            }
            else
            {
                smin = sindex + 1;
            }
        }
    }

    if (!found)
    {
        return false;
    }

    LumpInfo playpal{};

    if (!_lump_info(wad, "PLAYPAL", 0, playpal))
    {
        return false;
    }

    uint8_t* palette = (uint8_t*)(&wad->data[playpal.offset]);

    LumpInfo pnames{};

    if (!_lump_info(wad, "PNAMES", 0, pnames))
    {
        return false;
    }

    LumpInfo pstart{};

    if (!_lump_info(wad, "P_START", 0, pstart))
    {
        return false;
    }

    std::vector<gli::Pixel> pixels(texptr->width * texptr->height, gli::Pixel(0, 0, 0, 0));
    PatchDesc* patch = (PatchDesc*)(texptr + 1);

    for (int16_t i = 0; i < texptr->num_patches; ++i, ++patch)
    {
        size_t pname_offset = pnames.offset + sizeof(int) + patch->pname * sizeof(Wad::Name);
        Wad::Name pname = *((Wad::Name*)&wad->data[pname_offset]);

        LumpInfo picture{};

        if (!_lump_info(wad, pname, pstart.index, picture))
        {
            return false;
        }

        PicHeader* pheader = (PicHeader*)(&wad->data[picture.offset]);
        int* column_offsets = (int*)(pheader + 1);
        int x1 = patch->xpos;
        int x2 = std::min(x1 + pheader->width, (int)texptr->width);
        int x = std::max(x1, 0);

        for (; x < x2; ++x)
        {
            size_t column_data_offset = picture.offset + column_offsets[x - x1];
            uint8_t* column_data = (uint8_t*)&wad->data[column_data_offset];
            gli::Pixel* dest_column = &pixels[x * texptr->height];

            while (column_data[0] != 0xFF)
            {
                uint8_t ystart = column_data[0];
                uint8_t length = column_data[1];
                uint8_t* src = &column_data[3];
                int y = patch->ypos + ystart;

                if (y < 0)
                {
                    length -= y;
                    y = 0;
                }

                if (y + length > texptr->height)
                {
                    length = texptr->height - y;
                }

                while (length-- > 0)
                {
                    uint8_t palindex = *src++;
                    uint8_t* palentry = &palette[3 * palindex];
                    dest_column[y++] = gli::Pixel(palentry[0], palentry[1], palentry[2], 255);
                }

                column_data = column_data + column_data[1] + 4;
            }
        }
    }

    texture.width = texptr->height;
    texture.height = texptr->width;
    texture.pixels = std::move(pixels);

    return true;
}

} // namespace fist
