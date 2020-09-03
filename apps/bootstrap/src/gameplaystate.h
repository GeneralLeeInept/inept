#pragma once

#include "iappstate.h"
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

    void update_simulation(float delta);
    void render_game(float delta);
    void render_minimap(float delta);

    void draw_sprite(const V2f& position, Sprite sprite, int frame);
    void draw_nmi(const V2f& position);
    bool check_collision(const V2f& position, uint8_t filter_flags, float half_size);
    void move_movables(float delta);

    void fire_bullet(const Movable& attacker, const V2f& target);
    void update_bullets(float delta);

    std::vector<Movable> _movables;
    std::vector<AiBrain> _brains;
    std::vector<Bullet> _bullets;

    App* _app{};
    TileMap _tilemap;
    gli::Sprite _sprites[Sprite::Count];
    gli::Sprite _nmi_dark;
    gli::Sprite _nmi_shock;
    int _cx;
    int _cy;
    float _nmitimer;
    float _nmifired;
    float _simulation_delta;
    const TileMap::Zone* _player_zone;
    bool _map_view;
    bool _puzzle_mode;
    PuzzleState _puzzle_state;
};

} // namespace Bootstrap
