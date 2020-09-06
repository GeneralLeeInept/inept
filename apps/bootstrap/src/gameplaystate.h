#pragma once

#include "iappstate.h"
#include "puzzle.h"
#include "puzzlestate.h"
#include "tilemap.h"
#include "types.h"

#include <gli.h>

namespace Bootstrap
{

class GamePlayState : public IAppState
{
public:
    bool on_init(App* app) override;
    void on_destroy() override;
    bool on_enter() override;
    void on_exit() override;
    void on_suspend() override;
    void on_resume() override;
    bool on_update(float delta) override;

private:
    enum Sprite
    {
        Player,
        IllegalOpcode,
        Bullet_,
        MapMarkers,
        Hud,
        GameOver,
        PressStart,
        Victory,
        Count
    };

    struct Movable
    {
        bool active;
        Sprite sprite;
        V2f position;
        V2f velocity;
        float radius;
        int frame;
        uint8_t collision_flags;
    };

    struct AiBrain
    {
        float idle_timer;
        size_t movable;
        float next_think;
        V2f target_position;
        float player_spotted;
        float shot_timer;
    };

    struct Bullet
    {
        V2f position;
        V2f velocity;
        float radius;
    };

    struct Particle
    {
        V2f position;
        V2f velocity;
        float life;
        float max_life;
        Sprite sprite;
        int frame;
        int column;
    };

    void update_simulation(float delta);
    void update_ai(float delta);

    void render_game(float delta);
    void render_hud(float delta);
    void render_overlay(float delta, Sprite sprite);
    void render_minimap(float delta);

    void draw_sprite(const V2f& position, Sprite sprite, int frame);
    void draw_nmi(const V2f& position);
    bool check_collision(const V2f& position, uint8_t filter_flags, float half_size);
    void move_movables(float delta);

    void fire_bullet(const Movable& attacker, const V2f& target);
    void update_bullets(float delta);

    void spawn_particle_system(const Movable& movable);
    void update_particles(float delta);
    void render_particles();

    size_t find_enemy_in_range(const V2f& pos, float radius);

    bool on_screen(const Movable& movable, float hysterisis);

    std::vector<Puzzle::Definition> _puzzles;
    std::vector<Movable> _movables;
    std::vector<AiBrain> _brains;
    std::vector<Bullet> _bullets;
    std::vector<Particle> _particles;

    PuzzleState _puzzle_state;
    TileMap _tilemap;
    gli::Sprite _sprites[Sprite::Count];
    gli::Sprite _nmi_dark;
    gli::Sprite _nmi_shock;
    App* _app{};
    int _cx;
    int _cy;
    float _nmitimer;
    float _nmifired;
    float _simulation_delta;
    const TileMap::Zone* _player_zone;
    int _map_view;
    bool _puzzle_mode;
    size_t _puzzle_target;
    float _post_puzzle_cooloff;
    int _score;
    int _health;
    bool _game_over;
    size_t _next_puzzle;
    size_t _ais_remaining;
    bool _pre_start;
    float _state_transition_timer;
    float _hud_fade;
};

} // namespace Bootstrap
