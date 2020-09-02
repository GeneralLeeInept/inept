#pragma once

#include "iappstate.h"
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

    void draw_register(uint8_t reg, int x, int y, int color);
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
    gli::Sprite _status_panel;
    gli::Sprite _leds;
    gli::Sprite _nmi_dark;
    gli::Sprite _nmi_shock;
    int _cx;
    int _cy;
    uint8_t _dbus;
    uint8_t _abus_hi;
    uint8_t _abus_lo;
    uint8_t _a_reg;
    uint8_t _x_reg;
    uint8_t _y_reg;
    float _nmitimer;
    float _nmifired;
    float _simulation_delta;
    const TileMap::Zone* _player_zone;
};

} // namespace Bootstrap
