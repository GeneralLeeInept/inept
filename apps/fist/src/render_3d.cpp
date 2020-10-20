#include "render_3d.h"

#include "app.h"
#include "bsp.h"
#include "config.h"
#include "texture_manager.h"

#include <gli.h>

namespace fist
{

void Render3D::init(App* app)
{
    _app = app;
}

void Render3D::reload_config()
{
    _config_dirty = true;
}

void Render3D::draw_3d(const ThingPos& viewer, Map* map)
{
    if (_config_dirty)
    {
        load_configs();
    }

    // Handle screen size changes
    if (_screen_width != _app->screen_width() || _screen_height != _app->screen_height())
    {
        _screen_width = _app->screen_width();
        _screen_height = _app->screen_height();

        // Resize
        _floor_height.reset(new int[_screen_width]);
        _ceiling_height.reset(new int[_screen_width]);
        _solid_columns.reset(new SolidColumns[_screen_width]);
        _visplanes.reset();
        _max_visplanes = 0;

        // Recalculate
        _screen_aspect = (float)_screen_width / (float)_screen_height;
    }

    // Initialize frame state
    _map = map;

    _world_view = inverse(from_camera(viewer.p, viewer.f));

    for (int i = 0; i < _screen_width; ++i)
    {
        _floor_height[i] = _screen_height;
        _ceiling_height[i] = 0;
    }

    _num_visplanes = 0;
    _num_solid_columns = 2;
    _solid_columns[0].min = INT32_MIN;
    _solid_columns[0].max = -1;
    _solid_columns[0].next = &_solid_columns[1];
    _solid_columns[1].min = _screen_width;
    _solid_columns[1].max = INT32_MAX;
    _solid_columns[1].next = nullptr;

    draw_node(viewer, 0);

    for (size_t p = 0; p < _num_visplanes; ++p)
    {
        draw_plane(viewer, p);
    }
}

void Render3D::load_configs()
{
    // Load config values
    _fov_y = _app->config().get("render.fovy", 90.0f);
    _max_fade_dist = _app->config().get("render.fade_dist", 50.0f);
    _tex_scale = _app->config().get("render.tex_scale", 32.0f);

    // Update state
    _view_distance = std::tanf(_fov_y * 0.5f);
    _config_dirty = false;
}

void Render3D::grow_visplanes()
{
    _max_visplanes = _max_visplanes ? (_max_visplanes * 2) : 128;
    VisPlane* new_planes = new VisPlane[_max_visplanes];

    if (_num_visplanes)
    {
        gliAssert(_visplanes.get());

        for (size_t p = 0; p < _num_visplanes; ++p)
        {
            new_planes[p].texture_id = _visplanes[p].texture_id;
            new_planes[p].height = _visplanes[p].height;
            new_planes[p].light = _visplanes[p].light;
            new_planes[p].min_y = _visplanes[p].min_y;
            new_planes[p].max_y = _visplanes[p].max_y;
            new_planes[p].min_x = _visplanes[p].min_x;
            new_planes[p].max_x = _visplanes[p].max_x;
            new_planes[p].top = std::move(_visplanes[p].top);
            new_planes[p].bottom = std::move(_visplanes[p].bottom);
        }
    }

    for (size_t p = _num_visplanes; p < _max_visplanes; ++p)
    {
        new_planes[p].top = std::make_unique<int[]>(_screen_width);
        new_planes[p].bottom = std::make_unique<int[]>(_screen_width);
    }

    _visplanes.reset(new_planes);
}

size_t Render3D::find_visplane(uint64_t texture, float height, float light)
{
    size_t plane = 0;

    for (; plane < _num_visplanes; ++plane)
    {
        if (_visplanes[plane].texture_id == texture && _visplanes[plane].height == height && _visplanes[plane].light == light)
        {
            break;
        }
    }

    if (plane == _num_visplanes)
    {
        if (plane == _max_visplanes)
        {
            grow_visplanes();
        }

        _visplanes[plane].texture_id = texture;
        _visplanes[plane].height = height;
        _visplanes[plane].light = light;
        _visplanes[plane].min_x = _screen_width;
        _visplanes[plane].max_x = 0;
        _visplanes[plane].min_y = _screen_height;
        _visplanes[plane].max_y = 0;
        memset(_visplanes[plane].top.get(), 0, sizeof(int) * _screen_width);
        memset(_visplanes[plane].bottom.get(), 0xff, sizeof(int) * _screen_width);
        _num_visplanes++;
    }

    return plane;
}

size_t Render3D::check_visplane(size_t plane, int x, int top, int bottom)
{
    if (bottom <= top || plane == -1)
    {
        return plane;
    }

    if (_visplanes[plane].top[x] > 0 || _visplanes[plane].bottom[x] != -1)
    {
        size_t new_plane = _num_visplanes;

        if (new_plane == _max_visplanes)
        {
            grow_visplanes();
        }

        _visplanes[new_plane].texture_id = _visplanes[plane].texture_id;
        _visplanes[new_plane].height = _visplanes[plane].height;
        _visplanes[new_plane].light = _visplanes[plane].light;
        _visplanes[new_plane].min_x = _screen_width;
        _visplanes[new_plane].max_x = 0;
        _visplanes[new_plane].min_y = _screen_height;
        _visplanes[new_plane].max_y = 0;
        memset(_visplanes[new_plane].top.get(), 0, sizeof(int) * _screen_width);
        memset(_visplanes[new_plane].bottom.get(), 0xff, sizeof(int) * _screen_width);
        _num_visplanes++;
        plane = new_plane;
    }

    _visplanes[plane].min_y = std::min(_visplanes[plane].min_y, top);
    _visplanes[plane].max_y = std::max(_visplanes[plane].max_y, bottom);
    _visplanes[plane].min_x = std::min(_visplanes[plane].min_x, x);
    _visplanes[plane].max_x = std::max(_visplanes[plane].max_x, x);
    _visplanes[plane].top[x] = top;
    _visplanes[plane].bottom[x] = bottom;

    return plane;
}
void Render3D::mark_solid(int min, int max)
{
    SolidColumns* prev = nullptr;
    SolidColumns* solid = &_solid_columns[0];

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
            SolidColumns* new_solid = &_solid_columns[_num_solid_columns++];
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

bool Render3D::check_solid(int c)
{
    SolidColumns* solid = &_solid_columns[0];

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

static int side(const V2f& p, const V2f& split_normal, float split_distance)
{
    float pd = dot(split_normal, p);
    return (pd >= split_distance) ? 0 : 1;
}

void Render3D::draw_node(const ThingPos& viewer, uint32_t index)
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
        Node* node = &_map->nodes[index];
        int nearest = side(viewer.p, node->split_normal, node->split_distance);

        draw_node(viewer, node->child[nearest]);

        if (!cull_node(viewer, index, 1 - nearest))
        {
            draw_node(viewer, node->child[1 - nearest]);
        }
    }
}

bool Render3D::cull_node(const ThingPos& viewer, uint32_t index, int child)
{
    return false;
}

void Render3D::draw_subsector(const ThingPos& viewer, uint32_t index)
{
    SubSector* ss = &_map->subsectors[index];
    LineSeg* ls = &_map->linesegs[ss->first_seg];
    Sector* sector = &_map->sectors[ss->sector];

    if (sector->ceiling_height > viewer.h)
    {
        _ceiling_plane = find_visplane(sector->ceiling_texture, sector->ceiling_height, sector->light_level);
    }

    if (sector->floor_height < viewer.h)
    {
        _floor_plane = find_visplane(sector->floor_texture, sector->floor_height, sector->light_level);
    }

    for (uint32_t i = 0; i < ss->num_segs; ++i, ++ls)
    {
        draw_line(viewer, ls);
    }
}

void Render3D::draw_line(const ThingPos& viewer, const LineSeg* lineseg)
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

void Render3D::draw_solid_seg(const ThingPos& viewer, const LineSeg* lineseg)
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
    V2f from = _world_view * _map->vertices[lineseg->from];
    V2f to = _world_view * _map->vertices[lineseg->to];
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

        if (ci >= _floor_height[c] || fi <= _ceiling_height[c])
        {
            if (ci >= _floor_height[c])
            {
                _ceiling_plane = check_visplane(_ceiling_plane, c, _ceiling_height[c] - 1, _floor_height[c]);
                _ceiling_height[c] = _floor_height[c];
            }
            else
            {
                _floor_plane = check_visplane(_floor_plane, c, _ceiling_height[c] - 1, _floor_height[c]);
                _floor_height[c] = _ceiling_height[c];
            }

            // Height clipped
            ooy += dooydx;
            hc += dcdx;
            hf += dfdx;
            uooy += duooydx;
            continue;
        }

        if (ci < _ceiling_height[c])
        {
            ci = _ceiling_height[c];
        }

        if (fi > _floor_height[c])
        {
            fi = _floor_height[c];
        }

        if (fi > ci)
        {
            draw_column(c, 1.0f / ooy, uooy / ooy, hc, hf, texture_id, fade_offset, sector->light_level);
            _ceiling_plane = check_visplane(_ceiling_plane, c, _ceiling_height[c] - 1, ci);
            _floor_plane = check_visplane(_floor_plane, c, fi - 1, _floor_height[c]);
        }

        ooy += dooydx;
        hc += dcdx;
        hf += dfdx;
        uooy += duooydx;
    }

    mark_solid(ix1, ix2 - 1);
}

void Render3D::draw_non_solid_seg(const ThingPos& viewer, const LineSeg* lineseg)
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
    V2f from = _world_view * _map->vertices[lineseg->from];
    V2f to = _world_view * _map->vertices[lineseg->to];
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

            if (ci >= _floor_height[c])
            {
                // Height clipped
                _ceiling_plane = check_visplane(_ceiling_plane, c, _ceiling_height[c] - 1, _floor_height[c]);
                _ceiling_height[c] = _floor_height[c];
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            if (fi < _ceiling_height[c])
            {
                // Height clipped
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            if (ci < _ceiling_height[c])
            {
                ci = _ceiling_height[c];
            }

            if (fi > _floor_height[c])
            {
                fi = _floor_height[c];
            }

            if (fi > ci)
            {
                draw_column(c, 1.0f / ooy, uooy / ooy, hc, hf, texture_id, fade_offset, front_sector->light_level);
                _ceiling_plane = check_visplane(_ceiling_plane, c, _ceiling_height[c] - 1, ci);
                _ceiling_height[c] = fi;
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

            if (ci >= _floor_height[c])
            {
                _ceiling_plane = check_visplane(_ceiling_plane, c, _ceiling_height[c] - 1, _floor_height[c]);
                _ceiling_height[c] = _floor_height[c];
                hc += dcdx;
                continue;
            }

            if (ci > _ceiling_height[c])
            {
                _ceiling_plane = check_visplane(_ceiling_plane, c, _ceiling_height[c] - 1, ci);
                _ceiling_height[c] = ci;
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

            if (fi <= _ceiling_height[c])
            {
                _floor_plane = check_visplane(_floor_plane, c, _ceiling_height[c] - 1, _floor_height[c]);
                _floor_height[c] = _ceiling_height[c];
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            if (ci > _floor_height[c])
            {
                // Height clipped
                ooy += dooydx;
                hc += dcdx;
                hf += dfdx;
                uooy += duooydx;
                continue;
            }

            if (ci < _ceiling_height[c])
            {
                ci = _ceiling_height[c];
            }

            if (fi > _floor_height[c])
            {
                fi = _floor_height[c];
            }

            if (fi > ci)
            {
                draw_column(c, 1.0f / ooy, uooy / ooy, hc, hf, texture_id, fade_offset, front_sector->light_level);
                _floor_plane = check_visplane(_floor_plane, c, fi - 1, _floor_height[c]);
                _floor_height[c] = ci;
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

            if (ci <= _ceiling_height[c])
            {
                _floor_plane = check_visplane(_floor_plane, c, _ceiling_height[c] - 1, _floor_height[c]);
                _floor_height[c] = ci;
                hc += dcdx;
                continue;
            }

            if (ci < _floor_height[c])
            {
                _floor_plane = check_visplane(_floor_plane, c, ci - 1, _floor_height[c]);
                _floor_height[c] = ci;
            }

            hc += dcdx;
        }
    }
}

void Render3D::draw_column(int x, float dist, float texu, float top, float bottom, uint64_t texture_id, uint8_t fade_offset, float sector_light)
{
    int y1 = _app->screen_height() / 2 - (int)std::floor(top * _app->screen_height() * 0.5f + 0.5f);
    int y2 = _app->screen_height() / 2 - (int)std::floor(bottom * _app->screen_height() * 0.5f);
    gli::Sprite* texture = _app->texture_manager().get(texture_id);
    float fy1 = top * _app->screen_height() * 0.5f;
    //float dvdy = (2.0f * dist / _app->screen_height()) / _view_distance;
    float dvdy = (2.0f * dist) / (_view_distance * _app->screen_height());
    float texv = (fy1 - std::floor(fy1 + 0.5f)) * dvdy;

    if (y1 < _ceiling_height[x])
    {
        texv += dvdy * (_ceiling_height[x] - y1);
        y1 = _ceiling_height[x];
    }

    if (y2 > _floor_height[x])
    {
        y2 = _floor_height[x];
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

void Render3D::draw_plane(const ThingPos& viewer, size_t plane)
{
    VisPlane* vp = &_visplanes[plane];
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
                        _app->draw_line(span_start, y, span_end, y, gli::Pixel(255, 0, 255));
                    }
                }

                span_start = span_end + 1;
            }
        }
    }
}

} // namespace fist