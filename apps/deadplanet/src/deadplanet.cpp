#include <gli.h>

#include "tilemap.h"

class deadplanet : public gli::App
{
public:
    bool on_create() override
    {
        _tilemap.load("maps//deadworld.bin");
        return true;
    }

    void on_destroy() override {}

    DeadPlanet::TileMap _tilemap{};

    int _cx = 0;
    int _cy = 0;

    bool on_update(float delta) override
    {
        if (key_state(gli::Key::Key_W).down)
        {
            _cy -= 1;
        }

        if (key_state(gli::Key::Key_S).down)
        {
            _cy += 1;
        }

        if (key_state(gli::Key::Key_A).down)
        {
            _cx -= 1;
        }

        if (key_state(gli::Key::Key_D).down)
        {
            _cx += 1;
        }

        render();
        return true;
    }

    void render()
    {
        clear_screen(gli::Pixel(0x1F1D2C));

        //_cx = (int)(player.position.x * _tilemap.tile_size() + 0.5f) - _app->screen_width() / 2;
        //_cy = (int)(player.position.y * _tilemap.tile_size() + 0.5f) - _app->screen_height() / 2;

        int coarse_scroll_x = _cx / _tilemap.tile_size();
        int coarse_scroll_y = _cy / _tilemap.tile_size();
        int fine_scroll_x = _cx % _tilemap.tile_size();
        int fine_scroll_y = _cy % _tilemap.tile_size();

        for (int layer = 0; layer < 3; ++layer)
        {
            int sy = -fine_scroll_y;

            for (int y = coarse_scroll_y; y < _tilemap.height(); ++y)
            {
                int sx = -fine_scroll_x;

                for (int x = coarse_scroll_x; x < _tilemap.width(); ++x)
                {
                    int t = _tilemap(x, y, layer);

                    if (t)
                    {
                        int sheet;
                        int ox;
                        int oy;
                        bool has_alpha;
                        _tilemap.draw_info(t - 1, sheet, ox, oy, has_alpha);

                        if (layer > 0)
                        {
                            blend_partial_sprite(sx, sy, _tilemap.tilesheet(sheet).texture, ox, oy, _tilemap.tile_size(), _tilemap.tile_size(), 255);
                        }
                        else
                        {
                            draw_partial_sprite(sx, sy, &_tilemap.tilesheet(sheet).texture, ox, oy, _tilemap.tile_size(), _tilemap.tile_size());
                        }
                    }

                    sx += _tilemap.tile_size();

                    if (sx >= screen_width())
                    {
                        break;
                    }
                }

                sy += _tilemap.tile_size();

                if (sy >= screen_height())
                {
                    break;
                }
            }
        }
    }
};

deadplanet app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("deadplanet", 640, 360, 2))
    {
        app.run();
    }

    return 0;
}
