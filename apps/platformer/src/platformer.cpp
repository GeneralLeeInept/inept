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
        Ladder = 0x10,
        Water = 0x20,
        Door = 0x40,
        CollisionMask = 0xF
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

        TileRef& operator(int x, int y)
        {
            gliAssert(x >= 0 && x < Width && y >= 0 && y <= Height);
            return tiles[y * Width + x];
        }
    };

public:
    bool on_create() override
    {
        if (!tiles.load("res/nature-paltformer-tileset-16x16.png"))
        {
            return false;
        }

        return true;
    }

    void on_destroy() override {}

    bool on_update(float delta) override { return true; }
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
