#include "game_state.h"

#include "app.h"
#include "bsp_tree_builder.h"
#include "wad_loader.h"

#include <gli.h>

namespace fist
{

void GameState::on_init(App* app)
{
    _app = app;
    _render_3d.init(app);
    load_configs();
}

void GameState::on_pushed()
{
    _app->config().load();
    const std::string& wad_location =
            _app->config().get("game.wadfile", R"(C:\Program Files (x86)\Steam\SteamApps\common\Ultimate Doom\base\DOOM.WAD)");

    std::unique_ptr<WadFile, WadFileDeleter> wadfile(wad_open(wad_location));
    gliAssert(wadfile);

    bool result = _app->texture_manager().add_wad(wadfile.get());
    gliAssert(result && "Failed to load textures.");

    const std::string& map_name = _app->config().get("game.map", "E1M1");
    Wad::Map wad_map;
    result = wad_load_map(wadfile.get(), map_name.c_str(), wad_map);
    gliAssert(result && "Failed to load map.");

    _map = std::make_unique<fist::Map>();
    BspTreeBuilder::cook(wad_map, *_map);

    for (const Wad::ThingDef& thing : wad_map.things)
    {
        if (thing.type == Wad::ThingType::Player1Start)
        {
            _spawn_point.p = doom_to_world(thing.xpos, thing.ypos);
            _spawn_point.f = deg_to_rad((float)thing.angle);
            break;
        }
    }

    _player.pos = _spawn_point;

    const Sector* sector = sector_from_point(_player.pos.p);

    if (sector)
    {
        _player.pos.h = sector->floor_height + _eye_height;
    }
    else
    {
        _player.pos.h = _eye_height;
    }
}

void GameState::on_update(float delta)
{
    V2f move{};
    float delta_facing = 0.0f;

    if (_app->key_state(gli::Key_F5).pressed)
    {
        load_configs();
    }

    if (_app->key_state(gli::Key_W).down)
    {
        move.y += 1.0f;
    }

    if (_app->key_state(gli::Key_S).down)
    {
        move.y -= 1.0f;
    }

    if (_app->key_state(gli::Key_A).down)
    {
        move.x -= 1.0f;
    }

    if (_app->key_state(gli::Key_D).down)
    {
        move.x += 1.0f;
    }

    if (_app->key_state(gli::Key_Left).down)
    {
        delta_facing += 1.0f;
    }

    if (_app->key_state(gli::Key_Right).down)
    {
        delta_facing -= 1.0f;
    }

    _player.pos.f = _player.pos.f + delta_facing * deg_to_rad(_turn_speed) * delta;

    while (_player.pos.f < 0.0f)
    {
        _player.pos.f += 2.0f * PI;
    }

    while (_player.pos.f > 2.0f * PI)
    {
        _player.pos.f -= 2.0f * PI;
    }

    Transform2D t = from_camera(_player.pos.p, _player.pos.f);
    move = t.m * move * _move_speed * delta;
    _player.pos.p = _player.pos.p + move;

    if (_app->key_state(gli::Key_F3).pressed)
    {
        _player.pos.p = V2f { 95.98637390f, -103.15140533f };
        _player.pos.f = 4.60666800f;
    }

    // Get height based on sector
    const Sector* sector = sector_from_point(_player.pos.p);

    if (sector)
    {
        _player.pos.h = sector->floor_height + _eye_height;
    }

    render(delta);

    if (_app->key_state(gli::Key_PrintScreen).pressed)
    {
        _app->request_screenshot(".");
    }

    if (_app->key_state(gli::Key_F2).pressed)
    {
        gliLog(gli::LogLevel::Warning, "RenderIssue", "GameState::on_update", "pos: [%0.8f, %0.8f] facing: %0.8f", _player.pos.p.x, _player.pos.p.y,
               _player.pos.f);
    }
}

void GameState::load_configs()
{
    _app->config().load();
    _eye_height = _app->config().get("game.eye_height", 1.5f);
    _move_speed = _app->config().get("game.move_speed", 5.0f);
    _turn_speed = _app->config().get("game.turn_speed", 360.0f);
    _render_3d.reload_config();
}

V2f GameState::doom_to_world(int16_t x, int16_t y)
{
    return V2f{ (float)x, (float)y } / 32.0f;
}

int GameState::side(const V2f& p, const V2f& split_normal, float split_distance)
{
    float pd = dot(split_normal, p);
    return (pd >= split_distance) ? 0 : 1;
}

static float clearcolor_timer = 0.0f;

void GameState::render(float delta)
{
    _render_3d.draw_3d(_player.pos, _map.get());
}

const fist::Sector* GameState::sector_from_point(const V2f& p)
{
    uint32_t index = 0;
    const fist::Sector* sector = nullptr;

    while ((index & SubSectorBit) == 0)
    {
        int nearest = side(p, _map->nodes[index].split_normal, _map->nodes[index].split_distance);
        index = _map->nodes[index].child[nearest];
    }

    if (index != (uint32_t)-1)
    {
        uint32_t ssindex = index & ~SubSectorBit;
        fist::SubSector* subsector = &_map->subsectors[ssindex];
        sector = &_map->sectors[subsector->sector];
    }

    return sector;
}

} // namespace fist
