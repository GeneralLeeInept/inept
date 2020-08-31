#include "gameplaystate.h"

#include "assets.h"
#include "bootstrap.h"

namespace Bootstrap
{

bool GamePlayState::on_init(App* app)
{
    _app = app;

    if (!_tilemap.load(GliAssetPath("maps/main.bin")))
    {
        return false;
    }

    if (!_player.load(GliAssetPath("sprites/droid.png")))
    {
        return false;
    }

    if (!_status_panel.load(GliAssetPath("gui/status_panel.png")))
    {
        return false;
    }


    if (!_leds.load(GliAssetPath("gui/leds.png")))
    {
        return false;
    }

    return true;
}


void GamePlayState::on_destroy() {}


bool GamePlayState::on_enter()
{
    _cx = 0;
    _cy = 0;
    _px = 9 * _tilemap.tile_size() + (_tilemap.tile_size() >> 1);
    _py = 7 * _tilemap.tile_size() + (_tilemap.tile_size() >> 1);
    _dbus = 0xA5;
    _abus_hi = 0xDE;
    _abus_lo = 0xAD;
    _a_reg = 0xBE;
    _x_reg = 0x55;
    _y_reg = 0xAA;
    return true;
}


void GamePlayState::on_exit() {}


void GamePlayState::on_suspend() {}


void GamePlayState::on_resume() {}


bool GamePlayState::on_update(float delta)
{
    _app->clear_screen(gli::Pixel(0x1F1D2C));

    if (_app->key_state(gli::Key_W).down)
    {
        if (_py > 0)
        {
            _py -= 1;
        }
    }

    if (_app->key_state(gli::Key_S).down)
    {
        if (_py < (_tilemap.height() - 4) * _tilemap.tile_size())
        {
            _py += 1;
        }
    }

    if (_app->key_state(gli::Key_A).down)
    {
        if (_px > 0)
        {
            _px -= 1;
        }
    }

    if (_app->key_state(gli::Key_D).down)
    {
        if (_px < (_tilemap.width() - 4) * _tilemap.tile_size())
        {
            _px += 1;
        }
    }

    // playfield = 412 x 360
    _cx = _px - 206;
    _cy = _py - 180;

    int coarse_scroll_x = _cx / _tilemap.tile_size();
    int coarse_scroll_y = _cy / _tilemap.tile_size();
    int fine_scroll_x = _cx % _tilemap.tile_size();
    int fine_scroll_y = _cy % _tilemap.tile_size();

    int sy = -fine_scroll_y;

    for (int y = coarse_scroll_y; y < _tilemap.height(); ++y)
    {
        int sx = -fine_scroll_x;

        for (int x = coarse_scroll_x; x < _tilemap.width(); ++x)
        {
            int t = _tilemap(x, y);

            if (t)
            {
                int ox;
                int oy;
                bool has_alpha;
                _tilemap.tile_info(t - 1, ox, oy, has_alpha);

                if (has_alpha)
                {
                    _app->blend_partial_sprite(sx, sy, _tilemap.tilesheet(),  ox, oy, _tilemap.tile_size(), _tilemap.tile_size(), 255);
                }
                else
                {
                    _app->draw_partial_sprite(sx, sy, &_tilemap.tilesheet(), ox, oy, _tilemap.tile_size(), _tilemap.tile_size());
                }
            }

            sx += _tilemap.tile_size();

            if (sx >= _app->screen_width())
            {
                break;
            }
        }

        sy += _tilemap.tile_size();

        if (sy >= _app->screen_height())
        {
            break;
        }
    }

    int player_sx = _px - _cx;
    int player_sy = _py - _cy;
    _app->blend_sprite(player_sx, player_sy, _player, 255);

    _a_reg = _cx & 0xFF;
    _x_reg = _cy & 0xFF;

    _app->draw_sprite(412, 4, &_status_panel);
    draw_register(_dbus, 455, 48, 0);
    draw_register(_abus_lo, 455, 101, 2);
    draw_register(_abus_hi, 455, 120, 2);
    draw_register(_a_reg, 455, 167, 1);
    draw_register(_x_reg, 455, 214, 1);
    draw_register(_y_reg, 455, 261, 1);

    return true;
}


void GamePlayState::draw_register(uint8_t reg, int x, int y, int color)
{
    int ox = 0;
    int oy = color * 17;

    for (int i = 8; i--;)
    {
        uint8_t bit = (reg >> i) & 1;
        ox = bit * 17;
        _app->blend_partial_sprite(x, y, _leds, ox, oy, 17, 17, 255);
        x += 18;
    }
}

} // namespace Bootstrap