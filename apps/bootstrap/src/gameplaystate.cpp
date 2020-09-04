#include "gameplaystate.h"

#include "assets.h"
#include "collision.h"
#include "bootstrap.h"
#include "random.h"
#include "vga9.h"

namespace Bootstrap
{

bool GamePlayState::on_init(App* app)
{
    _app = app;

    static const char* sprite_sheet_paths[Sprite::Count] = { GliAssetPath("sprites/droid.png"), GliAssetPath("sprites/illegal_opcode.png"),
                                                             GliAssetPath("fx/bullet.png"), GliAssetPath("gui/map_markers.png") };

    size_t i = 0;

    for (gli::Sprite& sprite : _sprites)
    {
        if (!sprite.load(sprite_sheet_paths[i]))
        {
            return false;
        }

        i++;
    }

    if (!_nmi_dark.load(GliAssetPath("fx/nmi_dark.png")))
    {
        return false;
    }

    if (!_nmi_shock.load(GliAssetPath("fx/nmi_shock.png")))
    {
        return false;
    }

    if (!_tilemap.load_minimap(GliAssetPath("maps/6502.png")))
    {
        return false;
    }

    if (!_puzzle_state.on_init(app))
    {
        return false;
    }

    return true;
}


void GamePlayState::on_destroy()
{
    _puzzle_state.on_destroy();
}


bool GamePlayState::on_enter()
{
    if (!_tilemap.load(GliAssetPath("maps/6502.bin")))
    {
        return false;
    }

    _nmitimer = 0.0f;
    _nmifired = 0.0f;

    _movables.resize(1 + _tilemap.ai_spawns().size()); // Movable 0 is the player

    _movables[0].active = true;
    _movables[0].sprite = Sprite::Player;
    _movables[0].position = _tilemap.player_spawn();
    _movables[0].velocity = { 0.0f, 0.0f };
    _movables[0].radius = 11.0f / _tilemap.tile_size();
    _movables[0].frame = 1;
    _movables[0].collision_flags = TileMap::TileFlag::BlocksMovables;
    _player_zone = _tilemap.get_zone(_movables[0].position, nullptr);

    _brains.resize(_tilemap.ai_spawns().size());
    size_t ai_movable = 1;
    const std::vector<V2f>& ai_spawns = _tilemap.ai_spawns();

    for (AiBrain& brain : _brains)
    {
        Movable& movable = _movables[ai_movable];
        V2f spawn_position = ai_spawns[ai_movable - 1];
        brain.movable = ai_movable;
        brain.target_position = spawn_position;
        brain.shot_timer = 0.0f;
        movable.active = true;
        movable.sprite = Sprite::IllegalOpcode;
        movable.position = spawn_position;
        movable.velocity = { 0.0f, 0.0f };
        movable.radius = 11.0f / _tilemap.tile_size();
        movable.frame = 1;
        movable.collision_flags = TileMap::TileFlag::BlocksMovables | TileMap::TileFlag::BlocksAI;
        ai_movable++;
    }

    _simulation_delta = 0.0f;
    _map_view = false;

    return true;
}


void GamePlayState::on_exit()
{
    _movables.clear();

    if (_puzzle_mode)
    {
        _puzzle_state.on_exit();
    }
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
static const float NmiRadius = 1.5f;

// Physics constants
static const float min_v = 0.5f;
static const float drag = 0.95f;
static const float dv = 6.0f;

static const float bullet_speed = 10.0f; // experimentally derived

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
    if (_puzzle_target && !_puzzle_mode)
    {
        _puzzle_mode = _puzzle_state.on_enter();
    }

    if (_puzzle_mode)
    {
        if (!_puzzle_state.on_update(delta))
        {
            // Check result and react accordingly.
            bool success = _puzzle_state.result();
            _puzzle_state.on_exit();
            _puzzle_mode = false;
            _nmitimer = 0.0f;
            _bullets.clear();

            if (success)
            {
                _movables[_puzzle_target].active = false;
            }

            _puzzle_target = 0;
        }
    }

    if (!_puzzle_mode)
    {
        if (_map_view)
        {
            if (_app->key_state(gli::Key_Escape).pressed)
            {
                _map_view = false;
                delta = 0.0f;
            }
        }
        else if (!_map_view)
        {
            if (_app->key_state(gli::Key_M).pressed)
            {
                _map_view = true;
            }
        }

        if (_map_view)
        {
            render_minimap(delta);
        }
        else 
        {
            update_simulation(delta);
            render_game(delta);
        }
    }

    return true;
}

void GamePlayState::update_simulation(float delta)
{
    _simulation_delta += delta;

    // Simulate at 120Hz regardless of frame rate..
    while (_simulation_delta > 1.0f / 120.0f)
    {
        delta = 1.0f / 120.0f;
        _simulation_delta -= delta;

        // apply drag
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

                if (brain.shot_timer > 0.0f)
                {
                    brain.shot_timer -= delta;
                }

                if (brain.next_think <= 0.0f)
                {
                    // Time to consider..
                    float player_distance_sq = length_sq(player.position - movable.position);

                    if (player_distance_sq < (20.0f * 20.0f))
                    {
                        player_visible = !_tilemap.raycast(movable.position, player.position, TileMap::TileFlag::BlocksLos, nullptr);
                    }

                    if (player_visible)
                    {
                        brain.player_spotted = 1.0f;

                        if (player_distance_sq < (2.0f * 2.0f))
                        {
                            V2f dir = normalize(movable.position - player.position);
                            brain.target_position = player.position + dir * 4.0f;
                        }
                        else if (player_distance_sq > (5.0f * 5.0f))
                        {
                            V2f dir = normalize(movable.position - player.position);
                            brain.target_position = player.position + dir * 3.0f;
                        }
                        else
                        {
                            if (length_sq(player.velocity) > 1.0f)
                            {
                                brain.target_position = movable.position + V2f{ player.velocity.y, player.velocity.x };
                            }
                            else
                            {
                                brain.target_position = movable.position + V2f{ gRandom.get(), gRandom.get() };
                            }
                        }
                    }
                    else if (brain.player_spotted > delta)
                    {
                        brain.player_spotted -= delta;
                    }

                    brain.next_think += 0.5f;
                }

                // update velocity
                float distance_to_target_sq = length_sq(brain.target_position - movable.position);
                static const float MoveThreshold = 1.0f;

                if (distance_to_target_sq > (MoveThreshold * MoveThreshold))
                {
                    movable.velocity = normalize(brain.target_position - movable.position) * dv;
                }

                // update facing
                int facing = movable.frame & 1;

                if (player_visible)
                {
                    // Always face the player when in sight
                    facing = player.position.x > movable.position.x;
                }
                else if (std::abs(movable.velocity.x) > 0)
                {
                    // Face direction we're heading
                    facing = movable.velocity.x > 0;
                }

                movable.frame = brain.player_spotted ? 2 + facing : facing;

                // fire muh laser
                if (player_visible && brain.shot_timer <= 0.0f)
                {
                    float fire = gRandom.get();
                    bool fired = false;

                    if (fire > 0.8f)
                    {
                        V2f dir = normalize(player.position - movable.position);
                        V2f perp = V2f{ dir.y, dir.x };

                        // Fire a spread
                        int num = (fire > 0.9f) ? 5 : 3;

                        for (int i = -num / 2; i <= num / 2; ++i)
                        {
                            V2f shot_dir = dir + perp * (float)i;
                            fire_bullet(movable, movable.position + shot_dir);
                        }

                        fired = true;
                    }
                    else if (fire > 0.5f)
                    {
                        // Fire one
                        fire_bullet(movable, player.position);
                        fired = true;
                    }

                    if (fired)
                    {
                        brain.shot_timer = gRandom.get(0.3f, 0.8f);
                    }
                }
            }
        }

        move_movables(delta);

        _player_zone = _tilemap.get_zone(_movables[0].position, _player_zone);

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

            float invnmitimer = NmiDuration - _nmitimer;

            if (invnmitimer >= NmiZapInEnd && invnmitimer < NmiZapOutEnd)
            {
                // See if we caught an illegal opcode.
                _puzzle_target = find_enemy_in_range(player.position, NmiRadius);
            }
        }
        else
        {
            _nmitimer = 0.0f;

            if (_nmifired > 0.0f)
            {
                _nmitimer = NmiDuration;
                _nmifired = 0.0f;
            }
        }

        update_bullets(delta);
    }
}


void GamePlayState::render_game(float delta)
{
    _app->clear_screen(gli::Pixel(0x1F1D2C));

    Movable& player = _movables[0];

    // playfield = 412 x 360
    _cx = (int)(player.position.x * _tilemap.tile_size() + 0.5f) - 320;
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
                    _app->blend_partial_sprite(sx, sy, _tilemap.tilesheet(), ox, oy, _tilemap.tile_size(), _tilemap.tile_size(), 255);
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
    if (_nmitimer > 0.0f)
    {
        draw_nmi(player.position);
    }

    // sprites
    for (Movable& movable : _movables)
    {
        if (movable.active)
        {
            draw_sprite(movable.position, movable.sprite, movable.frame);
        }
    }

    // fx - post-sprites
    for (Bullet& bullet : _bullets)
    {
        draw_sprite(bullet.position, Sprite::Bullet_, 0);
    }

#if 0
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
#endif
}


void GamePlayState::render_minimap(float delta)
{
    _app->clear_screen(gli::Pixel(0x1F1D2C));

    int mx, my;
    mx = (_app->screen_width() - _tilemap.minimap().width()) / 2;
    my = (_app->screen_height() - _tilemap.minimap().height()) / 2;
    _app->blend_sprite(mx, my, _tilemap.minimap(), 255);

    int px, py;
    _tilemap.pos_to_minimap(_movables[0].position, px, py);
    _app->blend_partial_sprite(mx + px - 4, my + py - 4, _sprites[Sprite::MapMarkers], 0, 0, 8, 8, 255);

    if (_player_zone)
    {
        int w = (int)_player_zone->name.size() * vga9_glyph_width;
        int h = vga9_glyph_height;
        int x = (_app->screen_width() - w) / 2;
        int y = 300 - (h / 2);
        _app->draw_string(x, y, _player_zone->name.c_str(), vga9_glyphs, vga9_glyph_width, vga9_glyph_height, gli::Pixel(0xFFC0C0C0), gli::Pixel(0));
    }
}


void GamePlayState::draw_sprite(const V2f& position, Sprite sprite, int frame)
{
    int size = _sprites[sprite].height();
    int half_size = size / 2;
    int sx = (int)(position.x * _tilemap.tile_size() + 0.5f) - _cx - half_size;
    int sy = (int)(position.y * _tilemap.tile_size() + 0.5f) - _cy - half_size;
    _app->blend_partial_sprite(sx, sy, _sprites[sprite], frame * size, 0, size, size, 255);
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


bool GamePlayState::check_collision(const V2f& position, uint8_t filter_flags, float half_size)
{
    int sx = (int)std::floor(position.x - half_size);
    int sy = (int)std::floor(position.y - half_size);
    int ex = (int)std::floor(position.x + half_size);
    int ey = (int)std::floor(position.y + half_size);

    if (sx < 0 || sy < 0 || ex >= _tilemap.width() || ey >= _tilemap.height())
    {
        return true;
    }

    for (int tx = sx; tx <= ex; ++tx)
    {
        for (int ty = sy; ty <= ey; ++ty)
        {
            if ((_tilemap.tile_flags((uint16_t)tx, (uint16_t)ty) & filter_flags) != 0)
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

        if (check_collision(position + move, movable.collision_flags, movable.radius))
        {
            if (!check_collision(position + V2f{ move.x, 0.0f }, movable.collision_flags, movable.radius))
            {
                move.y = 0.0f;
                velocity.y = 0.0f;
            }
            else if (!check_collision(position + V2f{ 0.0f, move.y }, movable.collision_flags, movable.radius))
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

void GamePlayState::fire_bullet(const Movable& attacker, const V2f& target)
{
    V2f direction = normalize(target - attacker.position);
    Bullet bullet;
    bullet.position = attacker.position + direction * attacker.radius * 1.1f;
    bullet.velocity = direction * bullet_speed;
    bullet.radius = 0.05f;
    _bullets.push_back(bullet);
}

void GamePlayState::update_bullets(float delta)
{
    std::vector<Bullet> keep_bullets;
    keep_bullets.reserve(_bullets.size());

    for (Bullet& bullet : _bullets)
    {
        V2f next_position = bullet.position + bullet.velocity * delta;

        if (!check_collision(next_position, TileMap::TileFlag::BlocksBullets, bullet.radius))
        {
            // Check if hit player
            if (swept_circle_vs_circle(bullet.position, next_position, bullet.radius, _movables[0].position, _movables[0].radius))
            {
                // Hit the player
            }
            else
            {
                bullet.position = next_position;
                keep_bullets.push_back(bullet);
            }
        }
    }

    _bullets = keep_bullets;
}


size_t GamePlayState::find_enemy_in_range(const V2f& pos, float radius)
{
    float closest_d = radius * radius;
    size_t closest = 0;

    for (AiBrain& brain : _brains)
    {
        Movable& movable = _movables[brain.movable];

        if (movable.active)
        {
            V2f v = movable.position - pos;
            float d = length_sq(v);

            if (d < closest_d)
            {
                closest_d = d;
                closest = brain.movable;
            }
        }
    }

    return closest;
}


} // namespace Bootstrap
// namespace Bootstrap
