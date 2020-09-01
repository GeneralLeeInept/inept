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

    if (!_foe.load(GliAssetPath("sprites/illegal_opcode.png")))
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

    if (!_nmi_dark.load(GliAssetPath("fx/nmi_dark.png")))
    {
        return false;
    }

    if (!_nmi_shock.load(GliAssetPath("fx/nmi_shock.png")))
    {
        return false;
    }

    return true;
}


void GamePlayState::on_destroy() {}


bool GamePlayState::on_enter()
{
    _px = 9.5f * _tilemap.tile_size();
    _py = 8.5f * _tilemap.tile_size();
    _pvx = 0.0f;
    _pvy = 0.0f;
    _dbus = 0xA5;
    _abus_hi = 0xDE;
    _abus_lo = 0xAD;
    _a_reg = 0xBE;
    _x_reg = 0x55;
    _y_reg = 0xAA;
    _pfacing = 1;
    _nmitimer = 0.0f;
    _nmifired = 0.0f;
    return true;
}


void GamePlayState::on_exit() {}


void GamePlayState::on_suspend() {}


void GamePlayState::on_resume() {}

static const float NmiDarkInEnd = 0.15f;
static const float NmiZapInEnd = 0.2f;
static const float NmiZapOutStart = 0.4f;
static const float NmiZapOutEnd = 0.6f;
static const float NmiDarkOutEnd = 1.0f;
static const float NmiCooldown = 1.2f;
//static const float NmiDarkInEnd = 1.5f;
//static const float NmiZapInEnd = 2.f;
//static const float NmiZapOutStart = 4.f;
//static const float NmiZapOutEnd = 4.5f;
//static const float NmiDarkOutEnd = 6.0f;
//static const float NmiCooldown = 8.f;
static const float NmiDuration = NmiCooldown;

static float clamp(float t, float min, float max)
{
    return t < min ? min : (t > max ? max : t);
}
static float ease_in(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

static float ease_out(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(t, 3.0f);
}

bool GamePlayState::on_update(float delta)
{
    _app->clear_screen(gli::Pixel(0x1F1D2C));

    // Pixel units..
    static const float min_v = 16.0f;
    static const float drag = 0.95f;
    static const float dv = 192.0f;

    _pvx = std::abs(_pvx) < min_v ? 0.0f : _pvx * drag;
    _pvy = std::abs(_pvy) < min_v ? 0.0f : _pvy * drag;

    if (_app->key_state(gli::Key_W).down)
    {
        _pvy = -dv;
    }

    if (_app->key_state(gli::Key_S).down)
    {
        _pvy = dv;
    }

    if (_app->key_state(gli::Key_A).down)
    {
        _pvx = -dv;
    }

    if (_app->key_state(gli::Key_D).down)
    {
        _pvx = dv;
    }

    float move_x = _pvx * delta;
    float move_y = _pvy * delta;

    if (move_x < 0.0f)
    {
        _pfacing = 0;
    }
    else if (move_x > 0.0f)
    {
        _pfacing = 1;
    }

    // Clip movement
    static const float player_half_extent = 11.0f;

    if (check_collision(_px + move_x, _py + move_y, player_half_extent))
    {
        if (!check_collision(_px + move_x, _py, player_half_extent))
        {
            move_y = 0.0f;
            _pvy = 0.0f;
        }
        else if (!check_collision(_px, _py + move_y, player_half_extent))
        {
            move_x = 0.0f;
            _pvx = 0.0f;
        }
        else
        {
            move_x = 0.0f;
            move_y = 0.0f;
            _pvx = 0.0f;
            _pvy = 0.0f;
        }
    }

    _px += move_x;
    _py += move_y;

    if (_app->key_state(gli::Key_Space).pressed)
    {
        _nmifired = 0.5f;
    }
    else if (_nmifired >= delta)
    {
        _nmifired -= delta;
    }
    else
    {
        _nmifired = 0.0f;
    }

    if (_nmitimer >= delta)
    {
        _nmitimer -= delta;
    }
    else
    {
        _nmitimer = 0.0f;

        if (_nmifired > 0.0f)
        {
            _nmitimer = NmiDuration;
        }
    }

    // playfield = 412 x 360
    int cx = (int)(_px + 0.5f) - 206;
    int cy = (int)(_py + 0.5f) - 180;

    int coarse_scroll_x = cx / _tilemap.tile_size();
    int coarse_scroll_y = cy / _tilemap.tile_size();
    int fine_scroll_x = cx % _tilemap.tile_size();
    int fine_scroll_y = cy % _tilemap.tile_size();

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
                _tilemap.draw_info(t - 1, ox, oy, has_alpha);

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

    // fx - pre-sprites
    if (_nmitimer)
    {
        float nmistatetime = NmiDuration - _nmitimer;
        uint8_t nmidarkalpha = 0;

        if (nmistatetime < NmiDarkInEnd)
        {
            float t = ease_in(nmistatetime / NmiDarkInEnd);
            nmidarkalpha = (uint8_t)(t * 255.0f);
        }
        else if (nmistatetime < NmiZapOutEnd)
        {
            nmidarkalpha = 255;
        }
        else if (nmistatetime < NmiDarkOutEnd)
        {
            float t = ease_out((nmistatetime - NmiZapOutEnd) / (NmiDarkOutEnd - NmiZapOutEnd));
            nmidarkalpha = (uint8_t)(t * 255.0f);
        }

        if (nmidarkalpha)
        {
            int nmi_sx = (int)(_px + 0.5f) - cx;
            int nmi_sy = (int)(_py + 0.5f) - cy;
            _app->blend_sprite(nmi_sx - _nmi_dark.width() / 2, nmi_sy - _nmi_dark.height() / 2, _nmi_dark, nmidarkalpha);
        }

        uint8_t nmizapalpha = 0;

        if (nmistatetime >= NmiDarkInEnd)
        {
            if (nmistatetime < NmiZapInEnd)
            {
                float t = ease_in((nmistatetime - NmiDarkInEnd) / (NmiZapInEnd - NmiDarkInEnd));
                nmizapalpha = (uint8_t)(t * 255.0f);
            }
            else if (nmistatetime < NmiZapOutStart)
            {
                nmizapalpha = 255;
            }
            else if (nmistatetime < NmiZapOutEnd)
            {
                float t = ease_out((nmistatetime - NmiZapOutStart) / (NmiZapOutEnd - NmiZapOutStart));
                nmizapalpha = (uint8_t)(t * 255.0f);
            }

            if (nmizapalpha)
            {
                int nmi_sx = (int)(_px + 0.5f) - cx;
                int nmi_sy = (int)(_py + 0.5f) - cy;
                _app->blend_sprite(nmi_sx - _nmi_shock.width() / 2, nmi_sy - _nmi_shock.height() / 2, _nmi_shock, nmizapalpha);
            }
        }
    }

    // sprites
    static const int player_sprite_size = 32;
    int player_sx = (int)(_px + 0.5f) - cx;
    int player_sy = (int)(_py + 0.5f) - cy;
    _app->blend_partial_sprite(player_sx - player_sprite_size / 2, player_sy - player_sprite_size / 2, _player, _pfacing * player_sprite_size, 0,
                               player_sprite_size, player_sprite_size, 255);

    int foe_sx = (int)(9.5f * _tilemap.tile_size()) - cx;
    int foe_sy = (int)(28.5f * _tilemap.tile_size()) - cy;
    int foefacing = foe_sx < player_sx ? 1 : 0;
    _app->blend_partial_sprite(foe_sx - player_sprite_size / 2, foe_sy - player_sprite_size / 2, _foe, foefacing * player_sprite_size, 0,
                               player_sprite_size, player_sprite_size, 255);
    // fx - post-sprites


    // GUI
    _a_reg = cx & 0xFF;
    _x_reg = cy & 0xFF;

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


bool GamePlayState::check_collision(float x, float y, float half_size)
{
    int sx = std::floor(x - half_size) / _tilemap.tile_size();
    int sy = std::floor(y - half_size) / _tilemap.tile_size();
    int ex = std::floor(x + half_size) / _tilemap.tile_size();
    int ey = std::floor(y + half_size) / _tilemap.tile_size();

    if (sx < 0 || sx < 0 || ex >= _tilemap.width() || ey >= _tilemap.height())
    {
        return true;
    }

    for (int tx = sx; tx <= ex; ++tx)
    {
        for (int ty = sy; ty <= ey; ++ty)
        {
            if (!_tilemap.walkable((uint16_t)tx, (uint16_t)ty))
            {
                return true;
            }
        }
    }

    return false;
}


} // namespace Bootstrap
