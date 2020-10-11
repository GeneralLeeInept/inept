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
    load_configs();
}

void GameState::on_pushed()
{
    _app->config().load();
    const std::string& wad_location =
            _app->config().get("game.wadfile", R"(C:\Program Files (x86)\Steam\SteamApps\common\Ultimate Doom\base\DOOM.WAD)");
    const std::string& map_name = _app->config().get("game.map", "E1M1");

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

    // Get height based on sector
    const Sector* sector = sector_from_point(_player.pos.p);

    if (sector)
    {
        _player.pos.h = sector->floor_height + _eye_height;
    }

    render(delta);
}

void GameState::load_configs()
{
    _app->config().load();
    _fov_y = _app->config().get("render.fovy", 90.0f);
    _view_distance = std::tanf(_fov_y * 0.5f);
    _eye_height = _app->config().get("game.eye_height", 1.5f);
    _move_speed = _app->config().get("game.move_speed", 5.0f);
    _turn_speed = _app->config().get("game.turn_speed", 360.0f);
    _max_fade_dist = _app->config().get("render.fade_dist", 50.0f);
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
static int solid_columns[1280]{}; // FIXME: column per screen width
static int solid_value = 1;
static int floor_height[1280]{}; // current floor height per screen column
static int ceiling_height[1280]{}; // current ceiling height per screen column

void GameState::draw_3d(const ThingPos& viewer)
{
    // View matrix
    world_view = inverse(from_camera(viewer.p, viewer.f));
    
    for (int i = 0; i < 1280; ++i)
    {
        floor_height[i] = _app->screen_height();
        ceiling_height[i] = 0;
    }

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

    if (linedef->sidedefs[1] == (uint32_t)-1)
    {
        draw_solid_seg(viewer, lineseg);
    }
    else
    {
        draw_non_solid_seg(viewer, lineseg);
    }
}

void GameState::draw_solid_seg(const ThingPos& viewer, const LineSeg* lineseg)
{
    LineDef* linedef = &_map->linedefs[lineseg->line_def];
    SideDef* sidedef = &_map->sidedefs[linedef->sidedefs[lineseg->side]];
    Sector *sector = &_map->sectors[sidedef->sector];

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
    float ceil1 = ((sector->ceiling_height - viewer.h) * _view_distance * ooay);
    float ceil2 = ((sector->ceiling_height - viewer.h) * _view_distance * ooby);
    float floor1 = ((sector->floor_height - viewer.h) * _view_distance * ooay);
    float floor2 = ((sector->floor_height - viewer.h) * _view_distance * ooby);
    float dcdx = (ceil2 - ceil1) / (float)(ix2 - ix1);
    float dfdx = (floor2 - floor1) / (float)(ix2 - ix1);
    float dooydx = (ooby - ooay) / (float)(ix2 - ix1);

    if (ix2 > _app->screen_width())
    {
        ix2 = _app->screen_width();
    }

    float ooy = ooay;
    float hc = ceil1;
    float hf = floor1;

    if (ix1 < 0)
    {
        ooy += (-ix1) * dooydx;
        hc += (-ix1) * dcdx;
        hf += (-ix1) * dfdx;
        ix1 = 0;
    }


    for (int c = ix1; c < ix2; ++c)
    {
        if (solid_columns[c] == solid_value)
        {
            // depth buffer reject
            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            continue;
        }

        // Project wall column
        int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);
        int fi = _app->screen_height() / 2 - (int)std::floor(hf * _app->screen_height() * 0.5f);

        if (ci > floor_height[c] || fi < ceiling_height[c])
        {
            // Height clipped
            solid_columns[c] = solid_value;
            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            continue;
        }

        if (ci < ceiling_height[c])
        {
            ci = ceiling_height[c];
        }

        if (fi > floor_height[c])
        {
            fi = floor_height[c];
        }

        if (fi > ci)
        {
            float d = 1.0f / ooy;
            uint8_t fade = (uint8_t)std::floor(255.0f * (1.0f - clamp((d - _view_distance) / (_max_fade_dist - _view_distance), 0.2f, 1.0f)));
            _app->draw_line(c, ci, c, fi, gli::Pixel(0, 0, fade));
        }

        ooy += dooydx;
        hc += dcdx;
        hf += dfdx;

        // add to depth buffer
        solid_columns[c] = solid_value;
    }
}

void GameState::draw_non_solid_seg(const ThingPos& viewer, const LineSeg* lineseg)
{
    LineDef* linedef = &_map->linedefs[lineseg->line_def];
    SideDef* front_sidedef = &_map->sidedefs[linedef->sidedefs[lineseg->side]];
    SideDef* back_sidedef = &_map->sidedefs[linedef->sidedefs[1 - lineseg->side]];
    Sector* front_sector = &_map->sectors[front_sidedef->sector];
    Sector* back_sector = &_map->sectors[back_sidedef->sector];

    V2f v = _map->vertices[lineseg->from] - viewer.p;
    V2f l = _map->vertices[lineseg->to] - _map->vertices[lineseg->from];
    V2f n = { l.y, -l.x };
    bool front_facing = (dot(v, n) <= 0.0f);

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

    if (back_sector->ceiling_height < front_sector->ceiling_height)
    {
        // Draw upper columns
        int ix1 = (int)std::floor((x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        int ix2 = (int)std::floor((x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        float ceil1 = ((front_sector->ceiling_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((front_sector->ceiling_height - viewer.h) * _view_distance * ooby);
        float floor1 = ((back_sector->ceiling_height - viewer.h) * _view_distance * ooay);
        float floor2 = ((back_sector->ceiling_height - viewer.h) * _view_distance * ooby);
        float dcdx = (ceil2 - ceil1) / (float)(ix2 - ix1);
        float dfdx = (floor2 - floor1) / (float)(ix2 - ix1);
        float dooydx = (ooby - ooay) / (float)(ix2 - ix1);

        if (ix2 > _app->screen_width())
        {
            ix2 = _app->screen_width();
        }

        float ooy = ooay;
        float hc = ceil1;
        float hf = floor1;

        if (ix1 < 0)
        {
            ooy += (-ix1) * dooydx;
            hc += (-ix1) * dcdx;
            hf += (-ix1) * dfdx;
            ix1 = 0;
        }


        for (int c = ix1; c < ix2; ++c)
        {
            if (solid_columns[c] == solid_value)
            {
                // depth buffer reject
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);
            int fi = _app->screen_height() / 2 - (int)std::floor(hf * _app->screen_height() * 0.5f);

            if (ci > floor_height[c] || fi < ceiling_height[c])
            {
                // Height clipped
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                continue;
            }

            if (ci < ceiling_height[c])
            {
                ci = ceiling_height[c];
            }

            if (fi > floor_height[c])
            {
                fi = floor_height[c];
            }

            if (fi > ci)
            {
                float d = 1.0f / ooy;
                uint8_t fade = (uint8_t)std::floor(255.0f * (1.0f - clamp((d - _view_distance) / (_max_fade_dist - _view_distance), 0.2f, 1.0f)));
                _app->draw_line(c, ci, c, fi, gli::Pixel(fade, 0, 0));
                ceiling_height[c] = fi;
            }

            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
        }
    }
    else
    {
        // Just update column ceiling heights
        int ix1 = (int)std::floor((x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        int ix2 = (int)std::floor((x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        float ceil1 = ((front_sector->ceiling_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((front_sector->ceiling_height - viewer.h) * _view_distance * ooby);
        float hc = ceil1;
        float dcdx = (ceil2 - ceil1) / (float)(ix2 - ix1);

        if (ix1 < 0)
        {
            hc += (-ix1) * dcdx;
            ix1 = 0;
        }

        if (ix2 > _app->screen_width())
        {
            ix2 = _app->screen_width();
        }

        for (int c = ix1; c < ix2; ++c)
        {
            if (solid_columns[c] == solid_value)
            {
                // depth buffer reject
                hc += dcdx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);

            if (ci > ceiling_height[c])
            {
                ceiling_height[c] = ci;
            }

            hc += dcdx;
        }
    }

    if (front_facing && (back_sector->floor_height > front_sector->floor_height))
    {
        // Draw lower columns
        int ix1 = (int)std::floor((x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        int ix2 = (int)std::floor((x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        float ceil1 = ((back_sector->floor_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((back_sector->floor_height - viewer.h) * _view_distance * ooby);
        float floor1 = ((front_sector->floor_height - viewer.h) * _view_distance * ooay);
        float floor2 = ((front_sector->floor_height - viewer.h) * _view_distance * ooby);
        float dcdx = (ceil2 - ceil1) / (float)(ix2 - ix1);
        float dfdx = (floor2 - floor1) / (float)(ix2 - ix1);
        float dooydx = (ooby - ooay) / (float)(ix2 - ix1);

        if (ix2 > _app->screen_width())
        {
            ix2 = _app->screen_width();
        }

        float ooy = ooay;
        float hc = ceil1;
        float hf = floor1;

        if (ix1 < 0)
        {
            ooy += (-ix1) * dooydx;
            hc += (-ix1) * dcdx;
            hf += (-ix1) * dfdx;
            ix1 = 0;
        }


        for (int c = ix1; c < ix2; ++c)
        {
            if (solid_columns[c] == solid_value)
            {
                // depth buffer reject
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);
            int fi = _app->screen_height() / 2 - (int)std::floor(hf * _app->screen_height() * 0.5f);

            if (ci > floor_height[c] || fi < ceiling_height[c])
            {
                // Height clipped
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                continue;
            }

            if (ci < ceiling_height[c])
            {
                ci = ceiling_height[c];
            }

            if (fi > floor_height[c])
            {
                fi = floor_height[c];
            }

            if (fi > ci)
            {
                float d = 1.0f / ooy;
                uint8_t fade = (uint8_t)std::floor(255.0f * (1.0f - clamp((d - _view_distance) / (_max_fade_dist - _view_distance), 0.2f, 1.0f)));
                _app->draw_line(c, ci, c, fi, gli::Pixel(0, fade, 0));
                floor_height[c] = ci;
            }

            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
        }
    }
    else
    {
        // Just update column floor heights
        int ix1 = (int)std::floor((x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        int ix2 = (int)std::floor((x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f + 0.5f);
        float ceil1 = ((front_sector->floor_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((front_sector->floor_height - viewer.h) * _view_distance * ooby);
        float hc = ceil1;
        float dcdx = (ceil2 - ceil1) / (float)(ix2 - ix1);

        if (ix1 < 0)
        {
            hc += (-ix1) * dcdx;
            ix1 = 0;
        }

        if (ix2 > _app->screen_width())
        {
            ix2 = _app->screen_width();
        }

        for (int c = ix1; c < ix2; ++c)
        {
            if (solid_columns[c] == solid_value)
            {
                // depth buffer reject
                hc += dcdx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);

            if (ci < floor_height[c])
            {
                floor_height[c] = ci;
            }

            hc += dcdx;
        }
    }
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
