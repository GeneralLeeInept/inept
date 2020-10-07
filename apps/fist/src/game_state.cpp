#include "game_state.h"

#include "app.h"
#include "bsp_tree_builder.h"
#include "wad_loader.h"

#include <gli.h>

namespace fist
{

struct Mat2
{
    V2f x{ 1.0f, 0.0f };
    V2f y{ 0.0f, 1.0f };
};

static V2f operator*(const Mat2& m, const V2f& v)
{
    V2f result;
    result.x = dot(V2f{ m.x.x, m.y.x }, v);
    result.y = dot(V2f{ m.x.y, m.y.y }, v);
    return result;
}

struct Transform2D
{
    Mat2 m{};
    V2f p{};
};

static V2f operator*(const Transform2D& t, const V2f& p)
{
    return t.m * p + t.p;
}

static Mat2 transpose(const Mat2& m)
{
    Mat2 t;
    t.x.x = m.x.x;
    t.x.y = m.y.x;
    t.y.x = m.x.y;
    t.y.y = m.y.y;
    return t;
}

static Transform2D inverse(const Transform2D& t)
{
    Mat2 m = transpose(t.m);
    V2f p = -(m * t.p);
    return { m, p };
}

static Transform2D from_camera(const V2f& p, float facing)
{
    Transform2D t;
    float cos_facing = std::cos(facing);
    float sin_facing = std::sin(facing);

    // Forward
    t.m.y.x = cos_facing;
    t.m.y.y = sin_facing;

    // Right
    t.m.x.x = sin_facing;
    t.m.x.y = -cos_facing;

    t.p = p;
    return t;
}

void GameState::on_init(App* app)
{
    _app = app;
}

void GameState::on_pushed()
{
    _app->config().load();
    const std::string& wad_location =
            _app->config().get("game.wadfile", R"(C:\Program Files (x86)\Steam\SteamApps\common\Ultimate Doom\base\DOOM.WAD)");
    const std::string& map_name = _app->config().get("game.map", "E1M1");
    _fov_y = _app->config().get("render.fovy", 90.0f);
    _view_distance = std::tanf(_fov_y * 0.5f);

    _app->config().save();

    std::unique_ptr<WadFile, WadFileDeleter> wadfile(wad_open(wad_location));
    gliAssert(wadfile);

    Wad::Map wad_map;
    bool result = wad_load_map(wadfile.get(), map_name.c_str(), wad_map);
    gliAssert(result);

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
}

void GameState::on_update(float delta)
{
    const float move_speed = 1.4f;
    const float turn_speed = PI;
    V2f move{};
    float delta_facing = 0.0f;

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

    _player.pos.f = _player.pos.f + delta_facing * turn_speed * delta;

    while (_player.pos.f < 0.0f)
    {
        _player.pos.f += 2.0f * PI;
    }

    while (_player.pos.f > 2.0f * PI)
    {
        _player.pos.f -= 2.0f * PI;
    }

    Transform2D t = from_camera(_player.pos.p, _player.pos.f);
    move = t.m * move * move_speed * delta;
    _player.pos.p = _player.pos.p + move;

    render(delta);
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

void GameState::render(float delta)
{
    _app->clear_screen(gli::Pixel(128, 0, 128));
    _screen_aspect = (float)_app->screen_width() / (float)_app->screen_height();
    draw_3d(_player.pos);
}

static Transform2D world_view;
static int solid_columns[1280]{}; // 1 column per screen width
static int solid_value = 1;

void GameState::draw_3d(const ThingPos& viewer)
{
    // View matrix
    world_view = inverse(from_camera(viewer.p, viewer.f));
    draw_node(viewer, 0);
    solid_value = 1 - solid_value;
}

void GameState::draw_node(const ThingPos& viewer, uint32_t index)
{
    if (index & SubSectorBit)
    {
        if (index != (uint32_t)-1)
        {
            uint32_t ssindex = index & ~SubSectorBit;
            draw_subsector(viewer, ssindex);
        }
    }
    else
    {
        int nearest = side(viewer.p, _map->nodes[index].split_normal, _map->nodes[index].split_distance);

        if (_map->nodes[index].child[nearest] != (uint32_t)-1)
        {
            draw_node(viewer, _map->nodes[index].child[nearest]);
        }

        if (_map->nodes[index].child[1 - nearest] != (uint32_t)-1) 
        {
            draw_node(viewer, _map->nodes[index].child[1 - nearest]);
        }
    }
}

void GameState::draw_subsector(const ThingPos& viewer, uint32_t index)
{
    SubSector* ss = &_map->subsectors[index];
    LineSeg* ls = &_map->linesegs[ss->first_seg];

    for (uint32_t i = 0; i < ss->num_segs; ++i, ++ls)
    {
        draw_line(viewer, ls);
    }
}

void GameState::draw_line(const ThingPos& viewer, const LineSeg* lineseg)
{
    LineDef* linedef = &_map->linedefs[lineseg->line_def];

    if (linedef->sidedefs[1] != (uint32_t)-1)
    {
        return;
    }

    V2f v = _map->vertices[lineseg->from] - viewer.p;
    V2f l = _map->vertices[lineseg->to] - _map->vertices[lineseg->from];
    V2f n = { l.y, -l.x };

    if (dot(v, n) > 0.0f)
    {
        // Line is facing away from viewer
        return;
    }

    // Transform endpoints to view space
    V2f from = world_view * _map->vertices[lineseg->from];
    V2f to = world_view * _map->vertices[lineseg->to];

    // Near clip
    static const float near_clip = 0.1f;
    if (from.y < near_clip)
    {
        if (to.y <= near_clip)
        {
            return;
        }

        from.x = from.x + (near_clip - from.y) * (to.x - from.x) / (to.y - from.y);
        from.y = near_clip;
    }
    else if ((to.y < near_clip) && (from.y > near_clip))
    {
        to.x = to.x + (near_clip - to.y) * (from.x - to.x) / (from.y - to.y);
        to.y = near_clip;
    }

    // Project to viewport x
    float ooay = 1.0f / from.y;
    float ooby = 1.0f / to.y;
    float x1 = _view_distance * from.x * ooay;
    float x2 = _view_distance * to.x * ooby;

    if (x2 < x1)
    {
        std::swap(x1, x2);
        std::swap(ooay, ooby);
    }

    // Draw columns (use solid_column as a depth buffer)
    int ix1 = (int)std::floor((x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
    int ix2 = (int)std::floor((x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
    float h1 = _view_distance * ooay;
    float h2 = _view_distance * ooby;
    float dhdx = (h2 - h1) / (float)(ix2 - ix1);
    float dooydx = (ooby - ooay) / (float)(ix2 - ix1);

    if (ix2 > _app->screen_width())
    {
        ix2 = _app->screen_width();
    }

    float ooy = ooay;
    float h = h1;

    for (int c = ix1; c < ix2; ++c)
    {
        if (c < 0 || solid_columns[c] == solid_value)
        {
            // Left-clip / depth buffer reject
            ooy += dooydx;
            h += dhdx;
            continue;
        }

        // Project height (2m walls, height centered at eye)
        int y1 = _app->screen_height() / 2 - (int)std::floor(h * _app->screen_height() * 0.5f);
        int y2 = _app->screen_height() / 2 + (int)std::floor(h * _app->screen_height() * 0.5f);

        float d = 1.0f / ooy;
        uint8_t fade = (uint8_t)std::floor(255.0f * (1.0f - clamp((d - _view_distance) / (20.0f - _view_distance), 0.0f, 1.0f)));
        _app->draw_line(c, y1, c, y2, gli::Pixel(fade, fade, fade));
        ooy += dooydx;
        h += dhdx;

        // add to depth buffer
        solid_columns[c] = solid_value;
    }
}

} // namespace fist
