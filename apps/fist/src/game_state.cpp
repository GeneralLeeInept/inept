#include "game_state.h"

#include "app.h"
#include "bsp_tree_builder.h"
#include "wad_loader.h"

#include <gli.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

namespace fist
{

struct VisPlane
{
    uint64_t texture_id;
    float height;
    float light;
    int min_y, max_y;
    int min_x, max_x;
    int top[SCREEN_WIDTH];
    int bottom[SCREEN_WIDTH];
};

VisPlane* visplanes = nullptr;
size_t max_visplanes = 0;
size_t num_visplanes = 0;
size_t ceiling_plane;
size_t floor_plane;

size_t find_plane(uint64_t texture, float height, float light)
{
    size_t plane = 0;

    for (; plane < num_visplanes; ++plane)
    {
        if (visplanes[plane].texture_id == texture && visplanes[plane].height == height && visplanes[plane].light == light)
        {
            break;
        }
    }

    if (plane == num_visplanes)
    {
        if (plane == max_visplanes)
        {
            size_t new_max_visplanes = max_visplanes ? (max_visplanes << 1) : 16;
            visplanes = (VisPlane*)realloc(visplanes, new_max_visplanes * sizeof(VisPlane));
            gliAssert(visplanes);
            max_visplanes = new_max_visplanes;
        }

        visplanes[plane].texture_id = texture;
        visplanes[plane].height = height;
        visplanes[plane].light = light;
        visplanes[plane].min_y = SCREEN_HEIGHT;
        visplanes[plane].max_y = 0;
        visplanes[plane].min_x = SCREEN_WIDTH;
        visplanes[plane].max_x = 0;
        memset(visplanes[plane].top, 0, sizeof(visplanes[plane].top));
        memset(visplanes[plane].bottom, 0xff, sizeof(visplanes[plane].bottom));
        num_visplanes++;
    }

    return plane;
}

size_t check_plane(size_t plane, int x, int top, int bottom)
{
    if (bottom <= top || plane == -1)
    {
        return plane;
    }

    if (visplanes[plane].top[x] > 0 || visplanes[plane].bottom[x] != -1)
    {
        size_t new_plane = num_visplanes;

        if (new_plane == max_visplanes)
        {
            size_t new_max_visplanes = max_visplanes ? (max_visplanes << 1) : 16;
            visplanes = (VisPlane*)realloc(visplanes, new_max_visplanes * sizeof(VisPlane));
            gliAssert(visplanes);
            max_visplanes = new_max_visplanes;
        }

        visplanes[new_plane].texture_id = visplanes[plane].texture_id;
        visplanes[new_plane].height = visplanes[plane].height;
        visplanes[new_plane].light = visplanes[plane].light;
        visplanes[new_plane].min_y = SCREEN_HEIGHT;
        visplanes[new_plane].max_y = 0;
        visplanes[new_plane].min_x = SCREEN_WIDTH;
        visplanes[new_plane].max_x = 0;
        memset(visplanes[new_plane].top, 0, sizeof(visplanes[new_plane].top));
        memset(visplanes[new_plane].bottom, 0xff, sizeof(visplanes[new_plane].bottom));
        num_visplanes++;
        plane = new_plane;
    }

    visplanes[plane].min_y = std::min(visplanes[plane].min_y, top);
    visplanes[plane].max_y = std::max(visplanes[plane].max_y, bottom);
    visplanes[plane].min_x = std::min(visplanes[plane].min_x, x);
    visplanes[plane].max_x = std::max(visplanes[plane].max_x, x);
    visplanes[plane].top[x] = top;
    visplanes[plane].bottom[x] = bottom;

    return plane;
}

void GameState::draw_plane(const ThingPos& viewer, size_t plane, gli::Pixel color)
{
    VisPlane* vp = &visplanes[plane];
    gli::Sprite* texture = _app->texture_manager().get(vp->texture_id);
    float cos_facing = std::cos(viewer.f);
    float sin_facing = std::sin(viewer.f);

    for (int y = vp->min_y; y < vp->max_y; ++y)
    {
        if (vp->min_x <= vp->max_x)
        {
            int span_start = vp->min_x;
            int span_end = 0;

            float h = vp->height - viewer.h;
            float normy = (float)(_app->screen_height() - 2 * y) / (float)_app->screen_height();
            float dist = (_view_distance * h) / normy;

            V2f world{};
            V2f forward{ cos_facing, sin_facing };
            V2f right{ sin_facing, -cos_facing };
            world = viewer.p;
            world = world + forward * dist;
            float world_x = world.x;
            float world_y = world.y;

            float pixel_size = 1.0f / (float)_app->screen_height();
            float pixel_scale = pixel_size * (2.0f * dist) / _view_distance;
            float dudx = pixel_scale * sin_facing;
            float dvdx = pixel_scale * -cos_facing;
            float fade = clamp(1.0f - (dist / _max_fade_dist), 0.0f, 1.0f) * vp->light;

            while (span_start <= vp->max_x)
            {
                for (; span_start <= vp->max_x; ++span_start)
                {
                    if (vp->top[span_start] <= y && vp->bottom[span_start] >= y)
                    {
                        break;
                    }
                }

                for (span_end = span_start; span_end <= vp->max_x; ++span_end)
                {
                    if (vp->top[span_end] >= y || vp->bottom[span_end] <= y)
                    {
                        break;
                    }
                }

                if (span_end > span_start)
                {
                    if (texture)
                    {
                        float dx = (float)(span_start - _app->screen_width() * 0.5f);
                        float u = world_x + dx * dudx;
                        float v = world_y + dx * dvdx;
                        gli::Pixel* src = texture->pixels();
                        gli::Pixel* dest = _app->get_framebuffer() + (y * _app->screen_width()) + span_start;
                        for (int x = span_start; x < span_end; ++x)
                        {
                            int iu = (int)std::floor(u * _tex_scale + 0.5f) % 64;
                            int iv = (int)std::floor(v * _tex_scale + 0.5f) % 64;
                            if (iu < 0) iu += 64;
                            if (iv < 0) iv += 64;
                            *dest++ = src[iu + iv * 64] * fade;
                            u += dudx;
                            v += dvdx;
                        }
                    }
                    else
                    {
                        _app->draw_line(span_start, y, span_end, y, color);
                    }
                }

                span_start = span_end + 1;
            }
        }
    }
}

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
    _tex_scale = _app->config().get("render.tex_scale", 32.0f);
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
    clearcolor_timer += delta;

    while (clearcolor_timer > 5.0f)
    {
        clearcolor_timer -= 5.0f;
    }

    uint8_t clearintensity = (uint8_t)std::floor(64.0f + 25.6f * clearcolor_timer);
    //_app->clear_screen(gli::Pixel(clearintensity, 0, clearintensity));
    _screen_aspect = (float)_app->screen_width() / (float)_app->screen_height();
    draw_3d(_player.pos);
}

static Transform2D world_view;

struct SolidColumns
{
    int min;
    int max;
    SolidColumns* next;
};

static SolidColumns solid_columns[SCREEN_WIDTH];
static int num_solid_columns = 0;

void mark_solid(int min, int max)
{
    SolidColumns* prev = nullptr;
    SolidColumns* solid = &solid_columns[0];

    while (solid)
    {
        if (min - 1 > solid->max)
        {
            // Strictly after
            prev = solid;
            solid = solid->next;
            continue;
        }

        if (max + 1 < solid->min)
        {
            // Strictly before
            SolidColumns* new_solid = &solid_columns[num_solid_columns++];
            new_solid->min = min;
            new_solid->max = max;
            new_solid->next = solid;

            if (prev)
            {
                prev->next = new_solid;
            }

            break;
        }

        // Touching / overlapping, expand existing
        solid->min = std::min(solid->min, min);
        solid->max = std::max(solid->max, max);

        // Merge right
        SolidColumns* next = solid->next;

        while (next && (max + 1) >= next->min)
        {
            solid->max = std::max(solid->max, next->max);
            solid->next = next->next;
            next = solid->next;
        }

        break;
    }
}

bool check_solid(int c)
{
    SolidColumns* solid = &solid_columns[0];

    while (solid)
    {
        if (solid->min <= c && solid->max >= c)
        {
            return true;
        }
        solid = solid->next;
    }
    return false;
}

bool all_solid()
{
    return solid_columns[0].min = INT32_MIN && solid_columns[0].max == INT32_MAX;
}

static int floor_height[SCREEN_WIDTH]{};   // current floor height per screen column
static int ceiling_height[SCREEN_WIDTH]{}; // current ceiling height per screen column

void GameState::draw_3d(const ThingPos& viewer)
{
    // View matrix
    world_view = inverse(from_camera(viewer.p, viewer.f));

    for (int i = 0; i < SCREEN_WIDTH; ++i)
    {
        floor_height[i] = _app->screen_height();
        ceiling_height[i] = 0;
    }

    num_visplanes = 0;
    num_solid_columns = 2;
    solid_columns[0].min = INT32_MIN;
    solid_columns[0].max = -1;
    solid_columns[0].next = &solid_columns[1];
    solid_columns[1].min = SCREEN_WIDTH;
    solid_columns[1].max = INT32_MAX;
    solid_columns[1].next = nullptr;

    draw_node(viewer, 0);

    for (size_t p = 0; p < num_visplanes; ++p)
    {
        float norm_height = clamp(visplanes[p].height / 10.0f, -1.0f, 1.0f);
        uint8_t color = (uint8_t)std::floor(128.0f + norm_height * 127.0f);
        bool ceiling = visplanes[p].height < viewer.h;
        draw_plane(viewer, p, gli::Pixel(ceiling ? 0 : color, color >> 1, ceiling ? color : 0));
    }
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

        if (!all_solid() && !cull_node(viewer, index, nearest))
        {
            draw_node(viewer, _map->nodes[index].child[nearest]);
        }

        if (!all_solid() && !cull_node(viewer, index, 1 - nearest))
        {
            draw_node(viewer, _map->nodes[index].child[1 - nearest]);
        }
    }
}

bool GameState::cull_node(const ThingPos& viewer, uint32_t index, int child)
{
    if (_map->nodes[index].child[child] == (uint32_t)-1)
    {
        return false;
    }

    // Check if child's bounding box overlaps view region
    const BoundingBox& bbox = _map->nodes[index].bounds[child];
    V2f view_corners[4]{};

    view_corners[0] = world_view * bbox.mins;
    view_corners[1] = world_view * bbox.maxs;
    view_corners[2] = world_view * V2f{ bbox.mins.x, bbox.maxs.y };
    view_corners[3] = world_view * V2f{ bbox.maxs.x, bbox.mins.y };

    struct Split
    {
        V2f n;
        float d;
    };

    Split near_clip{};
    near_clip.n = V2f{ 0.0f, 1.0f };
    near_clip.d = 0.0f;
    bool culled = true;

    for (int i = 0; culled && i < 4; ++i)
    {
        culled = dot(near_clip.n, view_corners[i]) < near_clip.d;
    }

    if (culled)
    {
        return true;
    }

    float fov_x = deg_to_rad(_fov_y * _screen_aspect);
    float cf = std::cos(fov_x * 0.5f);
    float sf = std::sin(fov_x * 0.5f);

    Split left_clip{};
    left_clip.n = V2f{ cf, sf };
    left_clip.d = 0.0f;
    culled = true;

    for (int i = 0; culled && i < 4; ++i)
    {
        culled = dot(left_clip.n, view_corners[i]) < left_clip.d;
    }

    if (culled)
    {
        return true;
    }

    Split right_clip{};
    right_clip.n = V2f{ -cf, sf };
    right_clip.d = 0.0f;
    culled = true;

    for (int i = 0; culled && i < 4; ++i)
    {
        culled = dot(right_clip.n, view_corners[i]) < right_clip.d;
    }

    if (culled)
    {
        return true;
    }

    return false;
}

void GameState::draw_subsector(const ThingPos& viewer, uint32_t index)
{
    SubSector* ss = &_map->subsectors[index];
    LineSeg* ls = &_map->linesegs[ss->first_seg];
    Sector* sector = &_map->sectors[ss->sector];

    if (sector->ceiling_height > viewer.h)
    {
        ceiling_plane = find_plane(sector->ceiling_texture, sector->ceiling_height, sector->light_level);
    }
    else
    {
        ceiling_plane = -1;
    }

    if (sector->floor_height < viewer.h)
    {
        floor_plane = find_plane(sector->floor_texture, sector->floor_height, sector->light_level);
    }
    else
    {
        floor_plane = -1;
    }

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
    Sector* sector = &_map->sectors[sidedef->sector];

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
    V2f ln = normalize(l);
    float u_start = dot(ln, _map->vertices[lineseg->from]);
    float u_end = dot(ln, _map->vertices[lineseg->to]);

    if (u_start < u_end)
    {
        float offs{};
        u_start = std::modf(u_start, &offs);
        u_end -= offs;

        if (u_start < 0.0f)
        {
            u_start = u_start + 1.0f;
            u_end = u_end + 1.0f;
        }
    }
    else
    {
        float offs{};
        u_end = std::modf(u_end, &offs);
        u_start -= offs;

        if (u_end < 0.0f)
        {
            u_start = u_start + 1.0f;
            u_end = u_end + 1.0f;
        }
    }

    // Near clip
    static const float near_clip = 0.1f;
    if (from.y < near_clip)
    {
        if (to.y <= near_clip)
        {
            return;
        }

        float interp = (near_clip - from.y) / (to.y - from.y);
        from.x = from.x + interp * (to.x - from.x);
        from.y = near_clip;
        u_start = u_start + interp * (u_end - u_start);
    }
    else if ((to.y < near_clip) && (from.y > near_clip))
    {
        float interp = (near_clip - to.y) / (from.y - to.y);
        to.x = to.x + interp * (from.x - to.x);
        to.y = near_clip;
        u_end = u_end + interp * (u_start - u_end);
    }

    // Project to viewport x
    float ooay = 1.0f / from.y;
    float ooby = 1.0f / to.y;
    float x1 = _view_distance * from.x * ooay;
    float x2 = _view_distance * to.x * ooby;
    float u1 = u_start * ooay;
    float u2 = u_end * ooby;

    if (x2 < x1)
    {
        std::swap(x1, x2);
        std::swap(u1, u2);
        std::swap(ooay, ooby);
    }

    // Draw columns (use solid_column as a depth buffer)
    float fx1 = (x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
    float fx2 = (x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
    int ix1 = (int)std::floor(fx1 + 0.5f);
    int ix2 = (int)std::floor(fx2 + 0.5f);
    float ceil1 = ((sector->ceiling_height - viewer.h) * _view_distance * ooay);
    float ceil2 = ((sector->ceiling_height - viewer.h) * _view_distance * ooby);
    float floor1 = ((sector->floor_height - viewer.h) * _view_distance * ooay);
    float floor2 = ((sector->floor_height - viewer.h) * _view_distance * ooby);
    float dcdx = (ceil2 - ceil1) / (fx2 - fx1);
    float dfdx = (floor2 - floor1) / (fx2 - fx1);
    float dooydx = (ooby - ooay) / (fx2 - fx1);
    float dx = 0.5f + (float)ix1 - fx1;
    float ooy = ooay + dooydx * dx;
    float hc = ceil1 + dcdx * dx;
    float hf = floor1 + dfdx * dx;

    // Texture-mapping
    uint64_t texture_id = sidedef->textures[0];
    float uooy = u1;
    float duooydx = (u2 - u1) / (fx2 - fx1);

    if (ix2 > _app->screen_width())
    {
        ix2 = _app->screen_width();
    }

    if (ix1 < 0)
    {
        ooy += (-ix1) * dooydx;
        hc += (-ix1) * dcdx;
        hf += (-ix1) * dfdx;
        uooy += (-ix1) * duooydx;
        ix1 = 0;
    }

    uint8_t fade_offset = (_map->vertices[lineseg->from].x == _map->vertices[lineseg->to].x) ?
                                  20 :
                                  ((_map->vertices[lineseg->from].y == _map->vertices[lineseg->to].y) ? 10 : 0);

    for (int c = ix1; c < ix2; ++c)
    {
        if (check_solid(c))
        {
            // depth buffer reject
            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            uooy += duooydx;
            continue;
        }

        // Project wall column
        int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);
        int fi = _app->screen_height() / 2 - (int)std::floor(hf * _app->screen_height() * 0.5f);

        if (ci >= floor_height[c] || fi <= ceiling_height[c])
        {
            if (ci >= floor_height[c])
            {
                ceiling_plane = check_plane(ceiling_plane, c, ceiling_height[c] - 1, floor_height[c]);
                ceiling_height[c] = floor_height[c];
            }
            else
            {
                floor_plane = check_plane(floor_plane, c, ceiling_height[c] - 1, floor_height[c]);
                floor_height[c] = ceiling_height[c];
            }

            // Height clipped
            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            uooy += duooydx;
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
            draw_column(c, 1.0f / ooy, uooy / ooy, hc, hf, texture_id, fade_offset, sector->light_level);
            ceiling_plane = check_plane(ceiling_plane, c, ceiling_height[c] - 1, ci);
            floor_plane = check_plane(floor_plane, c, fi - 1, floor_height[c]);
        }

        ooy += dooydx;
        hc += dcdx;
        hf += dfdx;
        uooy += duooydx;
    }

    mark_solid(ix1, ix2 - 1);
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
    V2f ln = normalize(l);
    float u_start = dot(ln, _map->vertices[lineseg->from]);
    float u_end = dot(ln, _map->vertices[lineseg->to]);

    if (u_start < u_end)
    {
        float offs{};
        u_start = std::modf(u_start, &offs);
        u_end -= offs;

        if (u_start < 0.0f)
        {
            u_start = u_start + 1.0f;
            u_end = u_end + 1.0f;
        }
    }
    else
    {
        float offs{};
        u_end = std::modf(u_end, &offs);
        u_start -= offs;

        if (u_end < 0.0f)
        {
            u_start = u_start + 1.0f;
            u_end = u_end + 1.0f;
        }
    }

    // Near clip
    static const float near_clip = 0.1f;
    if (from.y < near_clip)
    {
        if (to.y <= near_clip)
        {
            return;
        }

        float interp = (near_clip - from.y) / (to.y - from.y);
        from.x = from.x + interp * (to.x - from.x);
        from.y = near_clip;
        u_start = u_start + interp * (u_end - u_start);
    }
    else if ((to.y < near_clip) && (from.y > near_clip))
    {
        float interp = (near_clip - to.y) / (from.y - to.y);
        to.x = to.x + interp * (from.x - to.x);
        to.y = near_clip;
        u_end = u_end + interp * (u_start - u_end);
    }

    // Project to viewport x
    float ooay = 1.0f / from.y;
    float ooby = 1.0f / to.y;
    float x1 = _view_distance * from.x * ooay;
    float x2 = _view_distance * to.x * ooby;
    float u1 = u_start * ooay;
    float u2 = u_end * ooby;

    if (x2 < x1)
    {
        std::swap(x1, x2);
        std::swap(u1, u2);
        std::swap(ooay, ooby);
    }

    uint8_t fade_offset = (_map->vertices[lineseg->from].x == _map->vertices[lineseg->to].x) ?
                                  20 :
                                  ((_map->vertices[lineseg->from].y == _map->vertices[lineseg->to].y) ? 10 : 0);

    if (back_sector->ceiling_height < front_sector->ceiling_height)
    {
        // Draw upper columns
        float fx1 = (x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        float fx2 = (x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        int ix1 = (int)std::floor(fx1 + 0.5f);
        int ix2 = (int)std::floor(fx2 + 0.5f);

        float top_height = front_sector->ceiling_height;
        float ceil1 = ((top_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((top_height - viewer.h) * _view_distance * ooby);

        float bottom_height = std::max(back_sector->ceiling_height, front_sector->floor_height);
        float floor1 = ((bottom_height - viewer.h) * _view_distance * ooay);
        float floor2 = ((bottom_height - viewer.h) * _view_distance * ooby);

        float dcdx = (ceil2 - ceil1) / (fx2 - fx1);
        float dfdx = (floor2 - floor1) / (fx2 - fx1);
        float dooydx = (ooby - ooay) / (fx2 - fx1);
        float dx = 0.5f + (float)ix1 - fx1;
        float ooy = ooay + dooydx * dx;
        float hc = ceil1 + dcdx * dx;
        float hf = floor1 + dfdx * dx;

        // Texture-mapping
        uint64_t texture_id = front_sidedef->textures[1];
        float uooy = u1;
        float duooydx = (u2 - u1) / (fx2 - fx1);

        if (ix2 > _app->screen_width())
        {
            ix2 = _app->screen_width();
        }

        if (ix1 < 0)
        {
            ooy += (-ix1) * dooydx;
            hc += (-ix1) * dcdx;
            hf += (-ix1) * dfdx;
            uooy += (-ix1) * duooydx;
            ix1 = 0;
        }


        for (int c = ix1; c < ix2; ++c)
        {
            if (check_solid(c))
            {
                // depth buffer reject
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);
            int fi = _app->screen_height() / 2 - (int)std::floor(hf * _app->screen_height() * 0.5f);

            if (ci >= floor_height[c])
            {
                // Height clipped
                ceiling_plane = check_plane(ceiling_plane, c, ceiling_height[c] - 1, floor_height[c]);
                ceiling_height[c] = floor_height[c];
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            if (fi < ceiling_height[c])
            {
                // Height clipped
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
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
                draw_column(c, 1.0f / ooy, uooy / ooy, hc, hf, texture_id, fade_offset, front_sector->light_level);
                ceiling_plane = check_plane(ceiling_plane, c, ceiling_height[c] - 1, ci);
                ceiling_height[c] = fi;
            }

            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            uooy += duooydx;
        }
    }
    else
    {
        // Just update column ceiling heights
        float fx1 = (x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        float fx2 = (x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        int ix1 = (int)std::floor(fx1 + 0.5f);
        int ix2 = (int)std::floor(fx2 + 0.5f);
        float ceil1 = ((front_sector->ceiling_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((front_sector->ceiling_height - viewer.h) * _view_distance * ooby);
        float dcdx = (ceil2 - ceil1) / (fx2 - fx1);
        float dx = 0.5f + (float)ix1 - fx1;
        float hc = ceil1 + dcdx * dx;

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
            if (check_solid(c))
            {
                // depth buffer reject
                hc += dcdx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);

            if (ci >= floor_height[c])
            {
                ceiling_plane = check_plane(ceiling_plane, c, ceiling_height[c] - 1, floor_height[c]);
                ceiling_height[c] = floor_height[c];
                hc += dcdx;
                continue;
            }

            if (ci > ceiling_height[c])
            {
                ceiling_plane = check_plane(ceiling_plane, c, ceiling_height[c] - 1, ci);
                ceiling_height[c] = ci;
            }

            hc += dcdx;
        }
    }

    if (front_facing && (back_sector->floor_height > front_sector->floor_height))
    {
        // Draw lower columns
        float fx1 = (x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        float fx2 = (x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        int ix1 = (int)std::floor(fx1 + 0.5f);
        int ix2 = (int)std::floor(fx2 + 0.5f);

        float top_height = std::min(front_sector->ceiling_height, back_sector->floor_height);
        float ceil1 = ((top_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((top_height - viewer.h) * _view_distance * ooby);

        float bottom_height = front_sector->floor_height;
        float floor1 = ((bottom_height - viewer.h) * _view_distance * ooay);
        float floor2 = ((bottom_height - viewer.h) * _view_distance * ooby);

        float dcdx = (ceil2 - ceil1) / (fx2 - fx1);
        float dfdx = (floor2 - floor1) / (fx2 - fx1);
        float dooydx = (ooby - ooay) / (fx2 - fx1);
        float dx = 0.5f + (float)ix1 - fx1;
        float ooy = ooay + dooydx * dx;
        float hc = ceil1 + dcdx * dx;
        float hf = floor1 + dfdx * dx;

        // Texture-mapping
        uint64_t texture_id = front_sidedef->textures[2];
        float uooy = u1;
        float duooydx = (u2 - u1) / (fx2 - fx1);

        if (ix2 > _app->screen_width())
        {
            ix2 = _app->screen_width();
        }

        if (ix1 < 0)
        {
            ooy += (-ix1) * dooydx;
            hc += (-ix1) * dcdx;
            hf += (-ix1) * dfdx;
            uooy += (-ix1) * duooydx;
            ix1 = 0;
        }


        for (int c = ix1; c < ix2; ++c)
        {
            if (check_solid(c))
            {
                // depth buffer reject
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);
            int fi = _app->screen_height() / 2 - (int)std::floor(hf * _app->screen_height() * 0.5f);

            if (fi <= ceiling_height[c])
            {
                floor_plane = check_plane(floor_plane, c, ceiling_height[c] - 1, floor_height[c]);
                floor_height[c] = ceiling_height[c];
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            if (ci > floor_height[c])
            {
                // Height clipped
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
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
                draw_column(c, 1.0f / ooy, uooy / ooy, hc, hf, texture_id, fade_offset, front_sector->light_level);
                floor_plane = check_plane(floor_plane, c, fi - 1, floor_height[c]);
                floor_height[c] = ci;
            }

            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            uooy += duooydx;
        }
    }
    else
    {
        // Just update column floor heights
        float fx1 = (x1 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        float fx2 = (x2 / _screen_aspect) * _app->screen_width() * 0.5f + _app->screen_width() * 0.5f;
        int ix1 = (int)std::floor(fx1 + 0.5f);
        int ix2 = (int)std::floor(fx2 + 0.5f);
        float ceil1 = ((front_sector->floor_height - viewer.h) * _view_distance * ooay);
        float ceil2 = ((front_sector->floor_height - viewer.h) * _view_distance * ooby);
        float dcdx = (ceil2 - ceil1) / (fx2 - fx1);
        float dx = 0.5f + (float)ix1 - fx1;
        float hc = ceil1 + dx * dcdx;

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
            if (check_solid(c))
            {
                // depth buffer reject
                hc += dcdx;
                continue;
            }

            // Project wall column
            int ci = _app->screen_height() / 2 - (int)std::floor(hc * _app->screen_height() * 0.5f);

            if (ci <= ceiling_height[c])
            {
                floor_plane = check_plane(floor_plane, c, ceiling_height[c] - 1, floor_height[c]);
                floor_height[c] = ci;
                hc += dcdx;
                continue;
            }

            if (ci < floor_height[c])
            {
                floor_plane = check_plane(floor_plane, c, ci - 1, floor_height[c]);
                floor_height[c] = ci;
            }

            hc += dcdx;
        }
    }
}

void GameState::draw_column(int x, float dist, float texu, float top, float bottom, uint64_t texture_id, uint8_t fade_offset, float sector_light)
{
    int y1 = _app->screen_height() / 2 - (int)std::floor(top * _app->screen_height() * 0.5f + 0.5f);
    int y2 = _app->screen_height() / 2 - (int)std::floor(bottom * _app->screen_height() * 0.5f);
    gli::Sprite* texture = _app->texture_manager().get(texture_id);
    float fy1 = top * _app->screen_height() * 0.5f;
    //float dvdy = (2.0f * dist / _app->screen_height()) / _view_distance;
    float dvdy = (2.0f * dist) / (_view_distance * _app->screen_height());
    float texv = (fy1 - std::floor(fy1 + 0.5f)) * dvdy;

    if (y1 < ceiling_height[x])
    {
        texv += dvdy * (ceiling_height[x] - y1);
        y1 = ceiling_height[x];
    }

    if (y2 > floor_height[x])
    {
        y2 = floor_height[x];
    }

    if (texture)
    {
        int dest_stride = _app->screen_width();
        gli::Pixel* dest = _app->get_framebuffer() + x + (y1 * dest_stride);
        int iu = (int)std::floor(texu * _tex_scale + 0.5f) % texture->height();
        gli::Pixel* src = texture->pixels() + iu * texture->width();

        float fade = (fade_offset / 255.0f) + clamp(1.0f - (dist / _max_fade_dist), 0.0f, 0.8f) * sector_light;

        for (int y = y1; y < y2; ++y, dest += dest_stride)
        {
            int iv = (int)std::floor(texv * _tex_scale + 0.5f) % texture->width();
            *dest = src[iv] * fade;
            texv += dvdy;
        }
    }
    else
    {
        uint8_t fade =
                fade_offset + (uint8_t)std::floor(235.0f * (1.0f - clamp((dist - _view_distance) / (_max_fade_dist - _view_distance), 0.0f, 1.0f)));
        _app->draw_line(x, y1, x, y2, gli::Pixel(fade, 0, fade));
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
