#include "gameplaystate.h"

#include "assets.h"
#include "collision.h"
#include "bootstrap.h"
#include "random.h"
#include "vga9.h"
#include "vread.h"

#include <algorithm>

namespace Bootstrap
{


enum HudGfx
{
    HealthBarX = 2,
    HealthBarY = 2,
    HealthBarW = 186,
    HealthBarH = 20,
    HealthFillX = 28,
    HealthFillY = 22,
    HealthFillW = 160,
    HealthFillH = 16,
    ScoreX = 193,
    ScoreY = 2,
    ScoreW = 144,
    ScoreH = 20,
    ScoreNumbersX = 337,
    ScoreNumbersY = 2,
    ScoreNumbersW = 24,
    ScoreNumbersH = 20,
    IllegalOpcodeX = 2,
    IllegalOpcodeY = 40,
    IllegalOpcodeW = 24,
    IllegalOpcodeH = 20,
};


static const float NmiDarkInEnd = 0.15f;
static const float NmiZapInEnd = 0.2f;
static const float NmiZapOutStart = 0.4f;
static const float NmiZapOutEnd = 0.6f;
static const float NmiDarkOutEnd = 1.0f;
static const float NmiCooldown = 1.2f;
static const float NmiDuration = NmiCooldown;
static const float NmiRadius = 1.5f;

static const float ParticleLifeMin = 0.2f;
static const float ParticleLifeMax = 0.5f;


// Physics constants
static const float min_v = 0.5f;
static const float drag = 0.95f;
static const float dv = 6.0f;

static const float bullet_speed = 10.0f; // experimentally derived


constexpr uint32_t puzzle_list_4cc()
{
    uint32_t magic = 0;
    return magic | ('P' << 24) | ('L' << 16) | ('S' << 8) | 'T';
}


std::vector<std::string> load_puzzle_list(const std::string& path)
{
    std::vector<uint8_t> data;
    std::vector<std::string> result;

    if (!GliFileSystem::get()->read_entire_file(path.c_str(), data))
    {
        return result;
    }

    size_t read_ptr = 0;
    uint32_t magic;

    if (!vread(magic, data, read_ptr) || magic != puzzle_list_4cc())
    {
        return result;
    }

    std::vector<std::string> temp;
    bool success = vread(temp, data, read_ptr);

    if (success)
    {
        result.reserve(temp.size());
        std::transform(temp.begin(), temp.end(), std::back_inserter(result), [](const std::string& path) -> std::string { return asset_path(path); });
    }

    return result;
}


bool GamePlayState::on_init(App* app)
{
    _app = app;

    static const char* sprite_sheet_paths[Sprite::Count] = { GliAssetPath("sprites/droid.png"), GliAssetPath("sprites/illegal_opcode.png"),
                                                             GliAssetPath("fx/bullet.png"),     GliAssetPath("gui/map_markers.png"),
                                                             GliAssetPath("gui/hud.png"),       GliAssetPath("gui/game_over.png"),
                                                             GliAssetPath("gui/title.png"),     GliAssetPath("gui/victory.png") };

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

    //// clang-format off
    //static const std::vector<const char*> puzzle_paths{
    //    GliAssetPath("puzzles/tax.bin"),
    //    GliAssetPath("puzzles/txa.bin"),
    //    GliAssetPath("puzzles/lda_immediate.bin"),
    //    GliAssetPath("puzzles/add.bin"),
    //    GliAssetPath("puzzles/sub.bin"),
    //    GliAssetPath("puzzles/inc.bin"),
    //    GliAssetPath("puzzles/ora_indexed.bin"),
    //    GliAssetPath("puzzles/jmp.bin"),
    //    GliAssetPath("puzzles/lda_indirect.bin"),
    //    GliAssetPath("puzzles/sta.bin"),
    //};
    //// clang-format on
    std::vector<std::string> puzzle_paths = load_puzzle_list(GliAssetPath("puzzles/puzzle_list.bin"));

    if (puzzle_paths.empty())
    {
        return false;
    }

    _puzzles.reserve(puzzle_paths.size());

    for (const std::string& path : puzzle_paths)
    {
        Puzzle::Definition definition;

        if (Puzzle::load(definition, path))
        {
            _puzzles.push_back(definition);
        }
        else
        {
            gli::logf("GamePlayState::on_init: Failed to load puzzle '%s'\n", path);
        }
    }

    if (_puzzles.empty())
    {
        return 0;
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
        brain.idle_timer = 0.0f;
        movable.active = true;
        movable.sprite = Sprite::IllegalOpcode;
        movable.position = spawn_position;
        movable.velocity = { 0.0f, 0.0f };
        movable.radius = 11.0f / _tilemap.tile_size();
        movable.frame = 1;
        movable.collision_flags = TileMap::TileFlag::BlocksMovables | TileMap::TileFlag::BlocksAI;
        ai_movable++;
    }

    _ais_remaining = _brains.size();
    _simulation_delta = 0.0f;
    _map_view = 0;
    _health = 100;
    _score = 0;
    _game_over = false;
    _next_puzzle = 0;
    _pre_start = true;
    _state_transition_timer = 0.0f;

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

static const float ShortFade = 0.125f;

bool GamePlayState::on_update(float delta)
{
    if (_game_over && _app->key_state(gli::Key_Space).pressed)
    {
        _app->set_next_state(AppState::InGame);
        return true;
    }
    else if (_pre_start && _app->key_state(gli::Key_Space).pressed)
    {
        _pre_start = false;
        _state_transition_timer = 0.0f;
        _hud_fade = ShortFade;
    }

    if (_pre_start)
    {
        render_game(delta);

        if (_state_transition_timer < 0.5f)
        {
            _app->set_screen_fade(App::BgColor, ease_out(_state_transition_timer * 2.0f));
        }
        else
        {
            render_overlay(_state_transition_timer - 1.0f, Sprite::PressStart);
        }

        _state_transition_timer += delta;

        return true;
    }
    else if (_game_over)
    {
        update_simulation(delta);
        update_particles(delta);
        render_game(delta);
        _state_transition_timer += delta;

        if (_ais_remaining == 0)
        {
            render_overlay(_state_transition_timer, Sprite::Victory);
        }
        else
        {
            render_overlay(_state_transition_timer, Sprite::GameOver);
        }
    }
    else if (_puzzle_target && !_puzzle_mode)
    {
        if (_state_transition_timer < ShortFade)
        {
            update_particles(delta);
            render_game(delta);
            render_hud(delta);
            _app->set_screen_fade(App::BgColor, ease_in(_state_transition_timer / ShortFade));
            _state_transition_timer += delta;
        }
        else
        {
            _puzzle_state.set_puzzle(_puzzles[_next_puzzle]);
            _puzzle_mode = _puzzle_state.on_enter();
            _app->set_screen_fade(App::BgColor, 1.0f);
        }
    }
    else
    {
        if (_puzzle_mode)
        {
            if (!_puzzle_state.on_update(delta))
            {
                // Check result and react accordingly.
                bool success = _puzzle_state.result();
                _puzzle_state.on_exit();
                _puzzle_mode = false;
                _nmitimer = 0.0f;
                _nmifired = 0.0f;
                _bullets.clear();
                _post_puzzle_cooloff = 0.5f;
                _state_transition_timer = 0.0f;
                _movables[0].velocity = V2f{};

                if (success)
                {
                    spawn_particle_system(_movables[_puzzle_target]);
                    _movables[_puzzle_target].active = false;
                    _score += 100;
                    _health += 20;
                    if (_health > 100) _health = 100;
                    _next_puzzle = (_next_puzzle + 1) % _puzzles.size();
                    --_ais_remaining;
                }
                else
                {
                    _health -= 20;
                }

                _puzzle_target = 0;
            }
        }

        if (!_puzzle_mode)
        {
            if (_post_puzzle_cooloff > 0.0f)
            {
                render_game(delta);
                render_hud(delta);

                _state_transition_timer += delta;
                _post_puzzle_cooloff -= delta;
                _app->set_screen_fade(App::BgColor, ease_out(_state_transition_timer / ShortFade));

                if (_post_puzzle_cooloff < 0.0f)
                {
                    _post_puzzle_cooloff = 0.0f;
                }
            }
            else
            {
                // _map_view: 0 - map not active, 1 - game fade out, 2 - map fade in, 3 - view map, 4 - map fade out, 5 - game fade in

                if (_map_view == 3)
                {
                    if (_app->key_state(gli::Key_Escape).pressed || _game_over)
                    {
                        _map_view = 4;
                        _state_transition_timer = 0.0f;
                    }
                }
                else if (_map_view == 0 && !_game_over)
                {
                    if (_app->key_state(gli::Key_M).pressed)
                    {
                        _map_view = 1;
                        _state_transition_timer = 0.0f;
                    }
                }

                if (_map_view)
                {
                    if (_map_view != 3)
                    {
                        if (_state_transition_timer >= ShortFade)
                        {
                            if (++_map_view == 6)
                            {
                                _map_view = 0;
                            }

                            _state_transition_timer = 0.0f;
                        }
                    }

                    if (_map_view == 0 || _map_view == 1 || _map_view == 5)
                    {
                        update_particles(delta);
                        render_game(delta);
                        render_hud(delta);
                    }
                    else
                    {
                        render_minimap(delta);
                    }

                    if (_map_view == 1 || _map_view == 4)
                    {
                        _app->set_screen_fade(App::BgColor, ease_in(_state_transition_timer / ShortFade));
                    }
                    else if (_map_view == 2 || _map_view == 5)
                    {
                        _app->set_screen_fade(App::BgColor, ease_out(_state_transition_timer / ShortFade));
                    }

                    _state_transition_timer += delta;
                }
                else
                {
                    update_simulation(delta);
                    update_particles(delta);
                    render_game(delta);
                    render_hud(delta);
                }
            }
        }
    }

    return true;
}

void GamePlayState::update_simulation(float delta)
{
    _simulation_delta += delta;

    if (_app->key_state(gli::Key_P).pressed)
    {
        spawn_particle_system(_movables[0]);
    }

    // Simulate at 120Hz regardless of frame rate..
    while (_simulation_delta > 1.0f / 120.0f)
    {
        if (!_game_over)
        {
            if (_health <= 0)
            {
                _movables[0].active = false;
                _game_over = true;
                _state_transition_timer = 0.0f;
                spawn_particle_system(_movables[0]);
            }
            else if (_ais_remaining == 0)
            {
                _game_over = true;
                _state_transition_timer = 0.0f;
            }
        }

        delta = 1.0f / 120.0f;
        _simulation_delta -= delta;

        // apply drag
        for (Movable& movable : _movables)
        {
            movable.velocity.x = std::abs(movable.velocity.x) < min_v ? 0.0f : movable.velocity.x * drag;
            movable.velocity.y = std::abs(movable.velocity.y) < min_v ? 0.0f : movable.velocity.y * drag;
        }

        Movable& player = _movables[0];

        if (player.active)
        {
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
        }

        update_ai(delta);

        move_movables(delta);

        _player_zone = _tilemap.get_zone(_movables[0].position, _player_zone);

        if (player.active && _app->key_state(gli::Key_Space).pressed)
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

            if (player.active && invnmitimer >= NmiZapInEnd && invnmitimer < NmiZapOutEnd)
            {
                // See if we caught an illegal opcode.
                _puzzle_target = find_enemy_in_range(player.position, NmiRadius);

                if (_puzzle_target)
                {
                    _state_transition_timer = 0.0f;
                }
            }
        }
        else
        {
            _nmitimer = 0.0f;

            if (player.active && _nmifired > 0.0f)
            {
                _nmitimer = NmiDuration;
                _nmifired = 0.0f;
            }
        }

        update_bullets(delta);
    }
}

void GamePlayState::update_ai(float delta)
{
    Movable& player = _movables[0];

    for (AiBrain& brain : _brains)
    {
        Movable& movable = _movables[brain.movable];

        if (!movable.active)
        {
            continue;
        }

        //float hysterisis = (brain.idle_timer > 0.0f) ? 32.0f : 0.0f;
        float hysterisis = -2.0f; //(brain.idle_timer > 0.0f) ? 32.0f : 0.0f;

        if (on_screen(movable, hysterisis))
        {
            brain.idle_timer = 0.5f;
        }

        if (brain.idle_timer > 0.0f)
        {
            brain.idle_timer -= delta;

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

                if (player.active && player_distance_sq < (20.0f * 20.0f))
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

            if (_post_puzzle_cooloff <= 0.0f)
            {
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
    }
}


void GamePlayState::render_game(float delta)
{
    _app->clear_screen(gli::Pixel(0x1F1D2C));

    Movable& player = _movables[0];

    _cx = (int)(player.position.x * _tilemap.tile_size() + 0.5f) - _app->screen_width() / 2;
    _cy = (int)(player.position.y * _tilemap.tile_size() + 0.5f) - _app->screen_height() / 2;

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
    render_particles();

    for (Bullet& bullet : _bullets)
    {
        draw_sprite(bullet.position, Sprite::Bullet_, 0);
    }
}


void GamePlayState::render_hud(float delta)
{
    uint8_t hud_alpha = (uint8_t)std::ceil((1.0f - ease_in(_hud_fade)) * 255.0f);

    // HUD
    int health_fill = (_health * HudGfx::HealthFillW) / 100;
    _app->blend_partial_sprite(16 + HudGfx::HealthBarW - HudGfx::HealthFillW, 16 + 3, _sprites[Sprite::Hud], HudGfx::HealthFillX, HudGfx::HealthFillY,
                               health_fill, HudGfx::HealthFillH, hud_alpha);
    _app->blend_partial_sprite(16, 16, _sprites[Sprite::Hud], HudGfx::HealthBarX, HudGfx::HealthBarY, HudGfx::HealthBarW, HudGfx::HealthBarH,
                               hud_alpha);

    int score_digits[4];
    if (_score >= 9999)
    {
        score_digits[0] = 9;
        score_digits[1] = 9;
        score_digits[2] = 9;
        score_digits[3] = 9;
    }
    else
    {
        int score_temp = _score;
        score_digits[0] = score_temp / 1000;
        score_temp -= score_digits[0] * 1000;
        score_digits[1] = score_temp / 100;
        score_temp -= score_digits[1] * 100;
        score_digits[2] = score_temp / 10;
        score_temp -= score_digits[2] * 10;
        score_digits[3] = score_temp;
    }

    int score_x = _app->screen_width() - (16 + HudGfx::ScoreNumbersW * 4);
    int score_y = 16 + (HudGfx::HealthBarH - HudGfx::ScoreNumbersH) / 2;

    for (int i = 0; i < 4; ++i)
    {
        _app->blend_partial_sprite(score_x, score_y, _sprites[Sprite::Hud], HudGfx::ScoreNumbersX + score_digits[i] * HudGfx::ScoreNumbersW,
                                   HudGfx::ScoreNumbersY, HudGfx::ScoreNumbersW, HudGfx::ScoreNumbersH, hud_alpha);
        score_x += HudGfx::ScoreNumbersW;
    }

    if (_ais_remaining >= 99)
    {
        score_digits[0] = 9;
        score_digits[1] = 9;
    }
    else
    {
        score_digits[0] = _ais_remaining / 10;
        score_digits[1] = _ais_remaining % 10;
    }

    int illegal_opcode_pad = 8;
    int remaining_x = _app->screen_width() - (16 + HudGfx::ScoreNumbersW * 2 + HudGfx::IllegalOpcodeW + illegal_opcode_pad);
    int remaining_y = _app->screen_height() - (16 + HudGfx::ScoreNumbersH);
    _app->blend_partial_sprite(remaining_x, remaining_y, _sprites[Sprite::Hud], HudGfx::IllegalOpcodeX, HudGfx::IllegalOpcodeY,
                               HudGfx::IllegalOpcodeW, HudGfx::IllegalOpcodeH, hud_alpha);
    remaining_x += HudGfx::IllegalOpcodeW + illegal_opcode_pad;

    for (int i = 0; i < 2; ++i)
    {
        _app->blend_partial_sprite(remaining_x, remaining_y, _sprites[Sprite::Hud], HudGfx::ScoreNumbersX + score_digits[i] * HudGfx::ScoreNumbersW,
                                   HudGfx::ScoreNumbersY, HudGfx::ScoreNumbersW, HudGfx::ScoreNumbersH, hud_alpha);
        remaining_x += HudGfx::ScoreNumbersW;
    }

    if (_hud_fade > delta)
    {
        _hud_fade -= delta;
    }
    else
    {
        _hud_fade = 0.0f;
    }
}


void GamePlayState::render_overlay(float delta, Sprite overlay)
{
    int x = (_app->screen_width() - _sprites[overlay].width()) / 2;
    int y = (_app->screen_height() - _sprites[overlay].height()) / 2;
    uint8_t overlay_fade = (uint8_t)std::ceil(ease_in(delta) * 255.0f);
    _app->blend_sprite(x, y, _sprites[overlay], overlay_fade);
}


void GamePlayState::render_minimap(float delta)
{
    _app->clear_screen(gli::Pixel(App::BgColor));

    int mx, my;
    mx = (_app->screen_width() - _tilemap.minimap().width()) / 2;
    my = (_app->screen_height() - _tilemap.minimap().height()) / 2;
    _app->blend_sprite(mx, my, _tilemap.minimap(), 255);

    int px, py;
    _tilemap.pos_to_minimap(_movables[0].position, px, py);
    _app->blend_partial_sprite(mx + px - 4, my + py - 4, _sprites[Sprite::MapMarkers], 0, 0, 8, 8, 255);

    for (const AiBrain& brain : _brains)
    {
        const Movable& movable = _movables[brain.movable];

        if (movable.active)
        {
            _tilemap.pos_to_minimap(movable.position, px, py);
            _app->blend_partial_sprite(mx + px - 4, my + py - 4, _sprites[Sprite::MapMarkers], 8, 0, 8, 8, 255);
        }
    }

    if (_player_zone)
    {
        int w = (int)_player_zone->name.size() * vga9_glyph_width;
        int h = vga9_glyph_height;
        int x = 32;
        int y = 150;
        _app->draw_text_box(x, y, 235, vga9_glyph_height, "Current location:", 0xFFE2E2E2, App::BgColor);
        y += vga9_glyph_height;
        _app->draw_text_box(x, y, 235, 205 - vga9_glyph_height, _player_zone->name.c_str(), 0xFFE2E2E2, App::BgColor);
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
            if (_movables[0].active &&
                swept_circle_vs_circle(bullet.position, next_position, bullet.radius, _movables[0].position, _movables[0].radius))
            {
                // Hit the player
                _health -= 5;
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


void GamePlayState::spawn_particle_system(const Movable& movable)
{
    gli::Sprite& sprite = _sprites[movable.sprite];
    int size = sprite.height();
    _particles.reserve(_particles.size() + size);
    V2f pos = (movable.position * _tilemap.tile_size()) + V2f{ size * -0.5f, size * -0.5f };

    pos.x += 48.0f;

    for (int i = 0; i < size; ++i)
    {
        Particle p;
        p.column = i;
        p.sprite = movable.sprite;
        p.frame = movable.frame;
        p.life = gRandom.get(ParticleLifeMin, ParticleLifeMax);
        p.max_life = p.life;
        p.velocity.y = -2.0f * sprite.height() / p.life;
        p.position = pos;
        pos.x = pos.x + 1.0f;
        _particles.push_back(p);
    }
}


void GamePlayState::update_particles(float delta)
{
    std::vector<Particle> keep_particles;
    keep_particles.reserve(_particles.size());

    for (Particle& particle : _particles)
    {
        if (particle.life > delta)
        {
            particle.life -= delta;
            particle.position.y += delta * particle.velocity.y;
            keep_particles.push_back(particle);
        }
    }

    _particles = keep_particles;
}


void GamePlayState::render_particles()
{
    for (Particle& p : _particles)
    {
        gli::Sprite& sprite = _sprites[p.sprite];
        int size = sprite.height();
        int sx = (int)(p.position.x + 0.5f) - _cx;
        int sy = (int)(p.position.y + 0.5f) - _cy;
        uint8_t alpha = std::ceil(ease_in(p.life / p.max_life) * 255.0f);
        _app->blend_partial_sprite(sx, sy, sprite, p.frame * size + p.column, 0, 1, size, alpha);
    }
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

bool GamePlayState::on_screen(const Movable& movable, float hysterisis)
{
    V2f screen_rect_expansion{ movable.radius + hysterisis, movable.radius + hysterisis };
    V2f screen_rect_origin{ 0.0f, 0.0f };
    V2f screen_rect_extent{ (float)_app->screen_width(), (float)_app->screen_height() };
    screen_rect_origin = screen_rect_origin - screen_rect_expansion * 0.5f;
    screen_rect_extent = screen_rect_extent + screen_rect_expansion;
    Rectf screen_rect = { screen_rect_origin, screen_rect_extent };
    V2f movable_screen_position = { movable.position.x * _tilemap.tile_size() - _cx, movable.position.y * _tilemap.tile_size() - _cy };
    return contains(screen_rect, movable_screen_position);
}

} // namespace Bootstrap
// namespace Bootstrap
