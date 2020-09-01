#pragma once

#include "iappstate.h"
#include "tilemap.h"

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
    struct V2f
    {
        float x;
        float y;
    };

    struct Movable
    {
        gli::Sprite* sprite;
        V2f position;
        V2f velocity;
        float radius;
        int frame;
    };

    void draw_register(uint8_t reg, int x, int y, int color);
    void draw_sprite(float x, float y, gli::Sprite& sheet, int frame);
    void draw_nmi(const V2f& position);
    bool check_collision(float x, float y, float half_size);
    void move_movables(float delta);

    std::vector<Movable> _movables;
    App* _app{};
    TileMap _tilemap;
    gli::Sprite _player;
    gli::Sprite _foe;
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
};

} // namespace Bootstrap
