#include <gli.h>

#include <vector>

class Platformer : public gli::App
{
    static const int TileSize = 16;

    enum TileFlags : uint8_t
    {
        CollisionTop = 0x1,
        CollisionBottom = 0x2,
        CollisionRight = 0x4,
        CollisionLeft = 0x8,
        CollisionAll = 0xF,
        Ladder = 0x10,
        Water = 0x20,
        Door = 0x40,
    };

    struct TileDef
    {
        int x;
        int y;
        uint8_t flags;
    };

    struct TileSheet
    {
        gli::Sprite gfx;
        std::vector<TileDef> defs;
    };

    struct TileRef
    {
        uint16_t tile_sheet_index;
        uint16_t tile_def_index;
    };

    template <int Width, int Height, int TileSheetCount>
    struct TileMap
    {
        TileSheet tile_sheets[TileSheetCount];
        TileRef tiles[Width * Height];

        constexpr int width() const { return Width; }
        constexpr int height() const { return Height; }

        TileSheet& tile_sheet(int index)
        {
            gliAssert(index >= 0 && index < TileSheetCount);
            return tile_sheets[index];
        }

        TileRef& operator()(int x, int y)
        {
            gliAssert(x >= 0 && x < Width && y >= 0 && y <= Height);
            return tiles[y * Width + x];
        }
    };

    TileMap<64, 16, 1> tile_map;

    bool create_tileset()
    {
        gli::Sprite& gfx = tile_map.tile_sheet(0).gfx;

        if (!gfx.load("res/nature-paltformer-tileset-16x16.png"))
        {
            return false;
        }

        std::vector<TileDef>& defs = tile_map.tile_sheet(0).defs;
        defs.resize(77);

        for (int y = 0; y < 11; ++y)
        {
            for (int x = 0; x < 7; ++x)
            {
                defs.at(y * 7 + x).x = x * TileSize;
                defs.at(y * 7 + x).y = y * TileSize;
            }
        }

        defs[0].flags = CollisionTop;
        defs[1].flags = CollisionTop;
        defs[2].flags = CollisionTop;
        defs[3].flags = CollisionTop;
        defs[4].flags = CollisionTop;
        defs[5].flags = CollisionTop;
        defs[6].flags = CollisionAll;

        defs[10].flags = CollisionTop;
        defs[11].flags = Door;
        defs[13].flags = CollisionAll;

        defs[17].flags = CollisionAll;
        defs[18].flags = CollisionAll;
        defs[19].flags = CollisionAll;
        defs[20].flags = CollisionAll;

        defs[21].flags = Ladder;
        defs[22].flags = Ladder;
        defs[24].flags = CollisionAll;
        defs[25].flags = CollisionAll;
        defs[26].flags = CollisionAll;
        defs[27].flags = CollisionAll;

        defs[28].flags = Ladder;
        defs[29].flags = Ladder;
        defs[31].flags = CollisionAll;
        defs[32].flags = CollisionAll;
        defs[33].flags = CollisionAll;
        defs[34].flags = CollisionAll;

        defs[35].flags = Ladder;
        defs[36].flags = Ladder;
        defs[40].flags = Water;
        defs[41].flags = CollisionAll;

        defs[42].flags = CollisionAll;
        defs[43].flags = CollisionAll;
        defs[44].flags = CollisionAll;

        defs[49].flags = CollisionAll;
        defs[50].flags = CollisionAll;

        return true;
    }

    const char* map_str =
        "                                                                "
        "                                                                "
        "                                                                "
        "                                                                "
        "                                                                "
        "               l                                                "
        "               k                                                "
        "               k           abbbbbbbbbbbc                        "
        "               k           deeeeeeeeeeef                        "
        "               j           deeeeeeeeeeef                        "
        "             mnnno         deeeeeeeeeeef                        "
        "        mnnno           abbbbceeeeeeeeef                        "
        "   mnnno                deeeefeeeeeeeeef                        "
        "                        deeeefeeeeeeeeef                        "
        "                        ghhhhieeeeeeeeef                        "
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    bool create_map()
    {
        const char* p = map_str;

        for (int y = 0; y < tile_map.height(); ++y)
        {
            for (int x = 0; x < tile_map.width(); ++x, ++p)
            {
                switch (*p)
                {
                    case ' ': tile_map(x, y).tile_def_index = 74; break;
                    case 'a': tile_map(x, y).tile_def_index = 0; break;
                    case 'b': tile_map(x, y).tile_def_index = 1; break;
                    case 'c': tile_map(x, y).tile_def_index = 2; break;
                    case 'd': tile_map(x, y).tile_def_index = 7; break;
                    case 'e': tile_map(x, y).tile_def_index = 8; break;
                    case 'f': tile_map(x, y).tile_def_index = 9; break;
                    case 'g': tile_map(x, y).tile_def_index = 14; break;
                    case 'h': tile_map(x, y).tile_def_index = 15; break;
                    case 'i': tile_map(x, y).tile_def_index = 16; break;
                    case 'j': tile_map(x, y).tile_def_index = 35; break;
                    case 'k': tile_map(x, y).tile_def_index = 28; break;
                    case 'l': tile_map(x, y).tile_def_index = 21; break;
                    case 'm': tile_map(x, y).tile_def_index = 42; break;
                    case 'n': tile_map(x, y).tile_def_index = 43; break;
                    case 'o': tile_map(x, y).tile_def_index = 44; break;
                    default: tile_map(x, y).tile_def_index = 76; break;
                }
            }
        }

        return true;
    }

public:
    bool on_create() override
    {
        if (!create_tileset())
        {
            return false;
        }

        if (!create_map())
        {
            return false;
        }

        return true;
    }

    void on_destroy() override {}

    bool on_update(float delta) override
    {
        for (int y = 0; y < tile_map.height(); ++y)
        {
            for (int x = 0; x < tile_map.width(); ++x)
            {
                const TileRef& tr = tile_map(x, y);
                const TileSheet& ts = tile_map.tile_sheet(tr.tile_sheet_index);
                const TileDef& td = ts.defs.at(tr.tile_def_index);
                draw_partial_sprite(x * TileSize, y * TileSize, &ts.gfx, td.x, td.y, TileSize, TileSize);
            }
        }

        return true;
    }
};

Platformer app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Platformer", 640, 360, 2))
    {
        app.run();
    }

    return 0;
}
