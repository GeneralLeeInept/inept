#include "gameplaystate.h"

#include "assets.h"
#include "bootstrap.h"
#include "random.h"

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
    _dbus = 0xA5;
    _abus_hi = 0xDE;
    _abus_lo = 0xAD;
    _a_reg = 0xBE;
    _x_reg = 0x55;
    _y_reg = 0xAA;
    _nmitimer = 0.0f;
    _nmifired = 0.0f;

    _movables.resize(9); // player + 8 AIS
    _movables[0].active = true;
    _movables[0].sprite = &_player;
    _movables[0].position = _tilemap.player_spawn();
    _movables[0].velocity = { 0.0f, 0.0f };
    _movables[0].radius = 11.0f / _tilemap.tile_size();
    _movables[0].frame = 1;

    _brains.resize(8);
    size_t ai_movable = 1;
    const std::vector<V2f>& ai_spawns = _tilemap.ai_spawns();

    for (AiBrain& brain : _brains)
    {
        Movable& movable = _movables[ai_movable];
        V2f spawn_position = ai_spawns[ai_movable - 1];
        brain.movable = ai_movable;
        brain.target_position = spawn_position;
        movable.active = true;
        movable.sprite = &_foe;
        movable.position = spawn_position;
        movable.velocity = { 0.0f, 0.0f };
        movable.radius = 11.0f / _tilemap.tile_size();
        movable.frame = 1;
        ai_movable++;
    }

    _app->show_mouse(true);

    return true;
}


void GamePlayState::on_exit()
{
    _movables.clear();
}


void GamePlayState::on_suspend() {}


void GamePlayState::on_resume() {}


static const float NmiDarkInEnd = 0.15f;
static const float NmiZapInEnd = 0.2f;
static const float NmiZapOutStart = 0.4f;
static const float NmiZapOutEnd = 0.6f;
static const float NmiDarkOutEnd = 1.0f;
static const float NmiCooldown = 1.2f;
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

    static const float min_v = 0.5f;
    static const float drag = 0.95f;
    static const float dv = 6.0f;

    // apply drag - FIXME - this is frame rate sensitive at the moment
    for (Movable& movable : _movables)
    {
        movable.velocity.x = std::abs(movable.velocity.x) < min_v ? 0.0f : movable.velocity.x * drag;
        movable.velocity.y = std::abs(movable.velocity.y) < min_v ? 0.0f : movable.velocity.y * drag;
    }

    Movable& player = _movables[0];

    if (_app->key_state(gli::Key_W).down)
    {
        player.velocity.y = -dv;
    }

    if (_app->key_state(gli::Key_S).down)
    {
        player.velocity.y = dv;
    }

    if (_app->key_state(gli::Key_A).down)
    {
        player.velocity.x = -dv;
    }

    if (_app->key_state(gli::Key_D).down)
    {
        player.velocity.x = dv;
    }

    if (player.velocity.x < 0.0f)
    {
        player.frame = 0;
    }
    else if (player.velocity.x > 0.0f)
    {
        player.frame = 1;
    }

    for (AiBrain& brain : _brains)
    {
        Movable& movable = _movables[brain.movable];

        if (movable.active)
        {
            bool player_visible = false;
            brain.next_think -= delta;

            if (brain.player_spotted > 0.0f)
            {
                brain.player_spotted -= delta;

                if (brain.player_spotted <= 0.0f)
                {
                    brain.player_spotted = 0.0f;
                    brain.target_position = movable.position;
                }
            }

            if (brain.next_think <= 0.0f)
            {
                // Time to consider..
                V2f player_direction = player.position - movable.position;
                float player_distance_sq = length_sq(player_direction);

                if (player_distance_sq < (10.0f * 10.0f))
                {
                    player_visible = !_tilemap.raycast(movable.position, player.position, nullptr);
                }

                if (player_visible)
                {
                    brain.player_spotted = 1.0f;
                    brain.target_position = player.position;
                }
                else if (brain.player_spotted > delta)
                {
                    brain.player_spotted -= delta;
                }

                brain.next_think += brain.player_spotted ? 0.1f : 0.5f;
            }

            float distance_to_target_sq = length_sq(brain.target_position - movable.position);
            static const float MoveThreshold = 1.0f;

            if (distance_to_target_sq > (MoveThreshold * MoveThreshold))
            {
                movable.velocity = normalize(brain.target_position - movable.position) * dv;
            }

            int facing = movable.frame & 1;

            if (std::abs(movable.velocity.x) > 0)
            {
                // Face direction we're heading
                facing = movable.velocity.x > 0;
            }
            else if (player_visible)
            {
                facing = player.position.x > movable.position.x;
            }

            movable.frame = brain.player_spotted ? 2 + facing : facing;
        }
    }

    move_movables(delta);

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
    _cx = (int)(player.position.x * _tilemap.tile_size() + 0.5f) - 206;
    _cy = (int)(player.position.y * _tilemap.tile_size() + 0.5f) - 180;

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
        draw_nmi(player.position);

    }

    // sprites
    for (Movable& movable : _movables)
    {
        if (movable.active)
        {
            draw_sprite(movable.position.x, movable.position.y, *movable.sprite, movable.frame);
        }
    }

    // fx - post-sprites

#if 0
    // Raycast test
    if (_app->mouse_state().buttons[0].down)
    {
        float denom = 1.0f / _tilemap.tile_size();
        V2f cursor_pos = V2f{ (float)_app->mouse_state().x, (float)_app->mouse_state().y };
        V2f player_screen_pos = (player.position * _tilemap.tile_size()) - V2f{ (float)_cx, (float)_cy };
        V2f screen_ray = (cursor_pos - player_screen_pos) * denom;
        _los_ray_start = player.position;
        _los_ray_hit = _tilemap.raycast(_los_ray_start, _los_ray_start + screen_ray, &_los_ray_end);
    }

    int lrx1 = (int)(_los_ray_start.x * _tilemap.tile_size() + 0.5f) - _cx;
    int lry1 = (int)(_los_ray_start.y * _tilemap.tile_size() + 0.5f) - _cy;
    int lrx2 = (int)(_los_ray_end.x * _tilemap.tile_size() + 0.5f) - _cx;
    int lry2 = (int)(_los_ray_end.y * _tilemap.tile_size() + 0.5f) - _cy;
    uint8_t lrcolor = _los_ray_hit ? 4 : 2;
    _app->draw_line(lrx1, lry1, lrx2, lry2, lrcolor);
#endif

    // GUI
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


void GamePlayState::draw_sprite(float x, float y, gli::Sprite& sheet, int index)
{
    int size = sheet.height();
    int half_size = size / 2;
    int sx = (int)(x * _tilemap.tile_size() + 0.5f) - _cx - half_size;
    int sy = (int)(y * _tilemap.tile_size() + 0.5f) - _cy - half_size;
    _app->blend_partial_sprite(sx, sy, sheet, index * size, 0, size, size, 255);
}


void GamePlayState::draw_nmi(const V2f& position)
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
        int nmi_sx = (int)(position.x * _tilemap.tile_size() + 0.5f) - _cx;
        int nmi_sy = (int)(position.y * _tilemap.tile_size() + 0.5f) - _cy;
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
            int nmi_sx = (int)(position.x * _tilemap.tile_size() + 0.5f) - _cx;
            int nmi_sy = (int)(position.y * _tilemap.tile_size() + 0.5f) - _cy;
            _app->blend_sprite(nmi_sx - _nmi_shock.width() / 2, nmi_sy - _nmi_shock.height() / 2, _nmi_shock, nmizapalpha);
        }
    }
}


bool GamePlayState::check_collision(float x, float y, float half_size)
{
    int sx = (int)std::floor(x - half_size);
    int sy = (int)std::floor(y - half_size);
    int ex = (int)std::floor(x + half_size);
    int ey = (int)std::floor(y + half_size);

    if (sx < 0 || sy < 0 || ex >= _tilemap.width() || ey >= _tilemap.height())
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


void GamePlayState::move_movables(float delta)
{
    std::vector<V2f> positions(_movables.size());
    std::vector<V2f> movements(_movables.size());
    std::vector<V2f> velocities(_movables.size());
    size_t i = 0;

    for (Movable& movable : _movables)
    {
        Movable& movable = _movables[i];
        V2f& position = positions[i];
        V2f& move = movements[i];
        V2f& velocity = velocities[i];

        position = movable.position;
        velocity = movable.velocity;
        move.x = velocity.x * delta;
        move.y = velocity.y * delta;

        if (check_collision(position.x + move.x, position.y + move.y, movable.radius))
        {
            if (!check_collision(position.x + move.x, position.y, movable.radius))
            {
                move.y = 0.0f;
                velocity.y = 0.0f;
            }
            else if (!check_collision(position.x, position.y + move.y, movable.radius))
            {
                move.x = 0.0f;
                velocity.x = 0.0f;
            }
            else
            {
                move.x = 0.0f;
                move.y = 0.0f;
                velocity.x = 0.0f;
                velocity.y = 0.0f;
            }
        }

        position.x += move.x;
        position.y += move.y;

        i++;
    }

    // Apply new positions, movements & velocities
    i = 0;

    for (Movable& movable : _movables)
    {
        movable.position = positions[i];
        movable.velocity = velocities[i];
        i++;
    }
}

}
// namespace Bootstrap
