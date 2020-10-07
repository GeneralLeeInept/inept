#pragma once

#include "appstate.h"
#include "bsp.h"
#include "types.h"

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
    struct ThingPos
    {
        V2f p{};
        float f{};
    };

    struct Player
    {
        ThingPos pos{};
    };

    App* _app{};
    std::unique_ptr<Map> _map{};
    Player _player{};
    ThingPos _spawn_point{};
    float _fov_y{};
    float _screen_aspect{};
    float _view_distance{};

    V2f doom_to_world(int16_t x, int16_t y);
    int side(const V2f& p, const V2f& split_normal, float split_distance);
    void render(float delta);
    void draw_3d(const ThingPos& viewer);
    void draw_node(const ThingPos& viewer, uint32_t index);
    void draw_subsector(const ThingPos& viewer, uint32_t index);
    void draw_line(const ThingPos& viewer, const LineSeg* lineseg);
};

}