#pragma once

#include "appstate.h"
#include "bsp.h"
#include "render_3d.h"
#include "types.h"

#include <gli.h>

#include <memory>

namespace fist
{

class GameState : public AppState
{
public:
    void on_init(App* app) override;
    void on_pushed() override;
    void on_update(float delta) override;

private:
    App* _app{};
    Render3D _render_3d{};
    std::unique_ptr<Map> _map{};
    Player _player{};
    ThingPos _spawn_point{};

    float _eye_height{};
    float _move_speed{};
    float _turn_speed{};

    void load_configs();
    V2f doom_to_world(int16_t x, int16_t y);
    int side(const V2f& p, const V2f& split_normal, float split_distance);
    void render(float delta);
    const Sector* sector_from_point(const V2f& p);
};

}