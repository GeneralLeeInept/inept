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
        _floor_mask.reset(new int[_screen_width]);
        _ceiling_mask.reset(new int[_screen_width]);
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
        _floor_mask[i] = _screen_height;
        _ceiling_mask[i] = -1;
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
    _near_clip = _app->config().get("render.near_clip", 0.1f);

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
        memset(_visplanes[plane].top.get(), 0xff, sizeof(int) * _screen_width);
        memset(_visplanes[plane].bottom.get(), 0xff, sizeof(int) * _screen_width);
        _num_visplanes++;
    }

    return plane;
}

size_t Render3D::check_visplane(size_t plane, int x_start, int x_end)
{
    if (plane == -1)
    {
        return plane;
    }

    for (int x = x_start; x <= x_end; ++x)
    {
        if (_visplanes[plane].top[x] != -1)
        {
            size_t new_plane = _num_visplanes;

            if (_num_visplanes == _max_visplanes)
            {
                grow_visplanes();
            }

            _visplanes[_num_visplanes].texture_id = _visplanes[plane].texture_id;
            _visplanes[_num_visplanes].height = _visplanes[plane].height;
            _visplanes[_num_visplanes].light = _visplanes[plane].light;
            _visplanes[_num_visplanes].min_x = x_start;
            _visplanes[_num_visplanes].max_x = x_end;
            _visplanes[_num_visplanes].min_y = _screen_height;
            _visplanes[_num_visplanes].max_y = 0;
            memset(_visplanes[_num_visplanes].top.get(), 0xff, sizeof(int) * _screen_width);
            memset(_visplanes[_num_visplanes].bottom.get(), 0xff, sizeof(int) * _screen_width);
            plane = _num_visplanes++;
            break;
        }
    }

    _visplanes[plane].min_x = std::min(_visplanes[plane].min_x, x_start);
    _visplanes[plane].max_x = std::max(_visplanes[plane].max_x, x_end);

    return plane;
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
    else
    {
        _ceiling_plane = -1;
    }

    if (sector->floor_height < viewer.h)
    {
        _floor_plane = find_visplane(sector->floor_texture, sector->floor_height, sector->light_level);
    }
    else
    {
        _floor_plane = -1;
    }

    for (uint32_t i = 0; i < ss->num_segs; ++i, ++ls)
    {
        draw_line(viewer, ls);
    }
}

void Render3D::draw_line(const ThingPos& viewer, const LineSeg* lineseg)
{
    LineDef* linedef = &_map->linedefs[lineseg->line_def];

    V2f seg_start = _map->vertices[lineseg->from];
    V2f seg_end = _map->vertices[lineseg->to];

    // Texture coordinates
    V2f v = seg_start - viewer.p;
    V2f l = seg_end - seg_start;
    V2f ln = normalize(l);
    float u_start = dot(ln, seg_start);
    float u_end = dot(ln, seg_end);

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

    // Transform to view space
    V2f view_start = _world_view * _map->vertices[lineseg->from];
    V2f view_end = _world_view * _map->vertices[lineseg->to];

    // Clip to near plane
    if (view_start.y < _near_clip)
    {
        if (view_end.y <= _near_clip)
        {
            // Near cull
            return;
        }

        float interp = (_near_clip - view_start.y) / (view_end.y - view_start.y);
        view_start.x = view_start.x + interp * (view_end.x - view_start.x);
        view_start.y = _near_clip;
        u_start = u_start + interp * (u_end - u_start);
    }
    else if (view_end.y < _near_clip && view_start.y > _near_clip)
    {
        float interp = (_near_clip - view_end.y) / (view_start.y - view_end.y);
        view_end.x = view_end.x + interp * (view_start.x - view_end.x);
        view_end.y = _near_clip;
        u_end = u_end + interp * (u_start - u_end);
    }

    // Project
    float ooy_start = 1.0f / view_start.y;
    float ooy_end = 1.0f / view_end.y;
    float x1 = _view_distance * view_start.x * ooy_start;
    float x2 = _view_distance * view_end.x * ooy_end;
    float uooy_start = u_start * ooy_start;
    float uooy_end = u_end * ooy_end;

    if (x2 < x1)
    {
        std::swap(x1, x2);
        std::swap(uooy_start, uooy_end);
        std::swap(ooy_start, ooy_end);
    }

    if (x1 > _screen_aspect || x2 < -_screen_aspect)
    {
        // Cull left/right
        return;
    }

    SideDef* front_sidedef = &_map->sidedefs[linedef->sidedefs[lineseg->side]];
    SideDef* back_sidedef = nullptr;
    Sector* front_sector = &_map->sectors[front_sidedef->sector];
    Sector* back_sector = nullptr;
    bool solid = linedef->sidedefs[1] == ((uint32_t)-1);

    if (!solid)
    {
        back_sidedef = &_map->sidedefs[linedef->sidedefs[1 - lineseg->side]];
        back_sector = &_map->sectors[back_sidedef->sector];

        if (front_sector->floor_height >= back_sector->ceiling_height || back_sector->floor_height >= front_sector->ceiling_height)
        {
            solid = true;
        }
    }

    V2f n = { l.y, -l.x };

    bool backface = (dot(v, n) > 0.0f);

    if (solid && backface)
    {
        // Line is facing away from viewer
        return;
    }

    // Cache textures
    gli::Sprite* textures[3]{};
    textures[0] = _app->texture_manager().get(front_sidedef->textures[0]);
    bool markfloor = solid;
    bool markceiling = solid;

    if (back_sector)
    {
        if (!backface && back_sector->ceiling_height < front_sector->ceiling_height)
        {
            textures[1] = _app->texture_manager().get(front_sidedef->textures[1]);
        }

        if (!backface && back_sector->floor_height > front_sector->floor_height)
        {
            textures[2] = _app->texture_manager().get(front_sidedef->textures[2]);
        }

        if (back_sector->ceiling_texture != front_sector->ceiling_texture || back_sector->ceiling_height != front_sector->ceiling_height ||
            back_sector->light_level != front_sector->light_level)
        {
            markceiling = true;
        }

        if (back_sector->floor_texture != front_sector->floor_texture || back_sector->floor_height != front_sector->floor_height ||
            back_sector->light_level != front_sector->light_level)
        {
            markfloor = true;
        }
    }

    // Setup
    float fx_start = (_screen_width * 0.5f) * ((x1 / _screen_aspect) + 1.0f);
    float fx_end = (_screen_width * 0.5f) * ((x2 / _screen_aspect) + 1.0f);
    float invdx = 1.0f / (fx_end - fx_start);
    float dooydx = (ooy_end - ooy_start) * invdx;
    float duooydx = (uooy_end - uooy_start) * invdx;

    // Top and bottom of middle section
    float top_start = (_screen_height * 0.5f) * (1.0f - (front_sector->ceiling_height - viewer.h) * _view_distance * ooy_start);
    float top_end = (_screen_height * 0.5f) * (1.0f - (front_sector->ceiling_height - viewer.h) * _view_distance * ooy_end);
    float dtopdx = (top_end - top_start) * invdx;

    float bottom_start = (_screen_height * 0.5f) * (1.0f - (front_sector->floor_height - viewer.h) * _view_distance * ooy_start);
    float bottom_end = (_screen_height * 0.5f) * (1.0f - (front_sector->floor_height - viewer.h) * _view_distance * ooy_end);
    float dbottomdx = (bottom_end - bottom_start) * invdx;

    // Bottom of top section
    float ceiling_start = top_start;
    float ceiling_end = 0.0f;
    float dceilingdx = dtopdx;

    if (textures[1])
    {
        ceiling_start = (_screen_height * 0.5f) * (1.0f - (back_sector->ceiling_height - viewer.h) * _view_distance * ooy_start);
        ceiling_end = (_screen_height * 0.5f) * (1.0f - (back_sector->ceiling_height - viewer.h) * _view_distance * ooy_end);
        dceilingdx = (ceiling_end - ceiling_start) * invdx;
    }

    // Top of bottom section
    float floor_start = bottom_start;
    float floor_end = 0.0f;
    float dfloordx = dbottomdx;

    if (textures[2])
    {
        floor_start = (_screen_height * 0.5f) * (1.0f - (back_sector->floor_height - viewer.h) * _view_distance * ooy_start);
        floor_end = (_screen_height * 0.5f) * (1.0f - (back_sector->floor_height - viewer.h) * _view_distance * ooy_end);
        dfloordx = (floor_end - floor_start) * invdx;
    }

    // Clip to solid columns and draw
    auto draw_columns = [&](int x_start, int x_end)
    {
        float dx = 0.5f + (float)x_start - fx_start;
        float ooy = ooy_start + dooydx * dx;
        float uooy = uooy_start + duooydx * dx;
        float top = top_start + dtopdx * dx;
        float bottom = bottom_start + dbottomdx * dx;
        float ceiling = ceiling_start + dceilingdx * dx;
        float floor = floor_start + dfloordx * dx;

        if (markceiling)
        {
            _ceiling_plane = check_visplane(_ceiling_plane, x_start, x_end);
        }

        if (markfloor)
        {
            _floor_plane = check_visplane(_floor_plane, x_start, x_end);
        }

        for (int x = x_start; x <= x_end; ++x)
        {
            if (textures[0])
            {
                draw_column(x, 1.0f / ooy, uooy / ooy, top, bottom, front_sidedef->textures[0], 0, 1.0f);
            }

            if (textures[1])
            {
                draw_column(x, 1.0f / ooy, uooy / ooy, top, ceiling, front_sidedef->textures[1], 0, 1.0f);
            }

            if (textures[2])
            {
                draw_column(x, 1.0f / ooy, uooy / ooy, floor, bottom, front_sidedef->textures[2], 0, 1.0f);
            }

            if (markceiling && _ceiling_plane != -1)
            {
                int iceiling_start = _ceiling_mask[x] + 1;
                int iceiling_end = (int)std::ceil(top - 0.5f);

                if (iceiling_end > _floor_mask[x])
                {
                    iceiling_end = _floor_mask[x];
                }

                if (iceiling_end > iceiling_start)
                {
                    _visplanes[_ceiling_plane].top[x] = iceiling_start;
                    _visplanes[_ceiling_plane].bottom[x] = iceiling_end;
                    _visplanes[_ceiling_plane].min_y = std::min(_visplanes[_ceiling_plane].min_y, iceiling_start);
                    _visplanes[_ceiling_plane].max_y = std::max(_visplanes[_ceiling_plane].max_y, iceiling_end);
                }
            }

            if (markfloor && _floor_plane != -1)
            {
                int ifloor_start = (int)std::floor(bottom - 0.5f) + 1;
                int ifloor_end = _floor_mask[x];

                if (ifloor_start < _ceiling_mask[x] + 1)
                {
                    ifloor_start = _ceiling_mask[x] + 1;
                }

                if (ifloor_end > ifloor_start)
                {
                    _visplanes[_floor_plane].top[x] = ifloor_start;
                    _visplanes[_floor_plane].bottom[x] = ifloor_end;
                    _visplanes[_floor_plane].min_y = std::min(_visplanes[_floor_plane].min_y, ifloor_start);
                    _visplanes[_floor_plane].max_y = std::max(_visplanes[_floor_plane].max_y, ifloor_end);
                }
            }

            if (markceiling)
            {
                int iceiling = (int)std::floor(ceiling - 0.5f) - 1;

                if (iceiling > _ceiling_mask[x])
                {
                    _ceiling_mask[x] = iceiling;
                }
            }

            if (markfloor)
            {
                int ifloor = (int)std::floor(floor - 0.5f) + 1;

                if (ifloor < _floor_mask[x])
                {
                    _floor_mask[x] = ifloor;
                }
            }

            ooy += dooydx;
            uooy += duooydx;
            top += dtopdx;
            bottom += dbottomdx;
            ceiling += dceilingdx;
            floor += dfloordx;
        }
    };

    int ix1 = (int)std::ceil(fx_start - 0.5f);
    int ix2 = (int)std::floor(fx_end - 0.5f);

    SolidColumns* solid_start = &_solid_columns[0];

    for (; solid_start && ix1 <= ix2;)
    {
        SolidColumns* solid_end = solid_start->next;

        if (solid_start->max > ix2 + 1)
        {
            break;
        }

        if (solid_end->min < ix1 - 1)
        {
            solid_start = solid_end;
            continue;
        }

        if (ix1 > solid_start->max + 1)
        {
            if (ix2 + 1 < solid_end->min)
            {
                // Span from ix1 to ix2 is visible
                draw_columns(ix1, ix2);

                if (solid)
                {
                    SolidColumns* solid_seg = &_solid_columns[_num_solid_columns++];
                    solid_seg->min = ix1;
                    solid_seg->max = ix2;
                    solid_seg->next = solid_end;
                    solid_start->next = solid_seg;
                }

                break;
            }
            else
            {
                // Span from ix1 to solid_end->min - 1 is visible
                draw_columns(ix1, solid_end->min - 1);

                if (solid)
                {
                    solid_end->min = ix1;
                }

                solid_start = solid_end;
            }
        }
        else
        {
            if (ix2 + 1 < solid_end->min)
            {
                // Span from solid_start->max + 1 to ix2 is visible
                draw_columns(solid_start->max + 1, ix2);

                if (solid)
                {
                    solid_start->max = ix2;
                }

                break;
            }
            else
            {
                // Span from solid_start->max + 1 to solid_end->min - 1 is visible
                draw_columns(solid_start->max + 1, solid_end->min - 1);

                if (solid)
                {
                    solid_start->max = solid_end->max;
                    solid_start->next = solid_end->next;
                }
                else
                {
                    solid_start = solid_end;
                }
            }
        }

        ix1 = solid_end->max + 1;
    }
}

void Render3D::draw_column(int x, float dist, float texu, float top, float bottom, uint64_t texture_id, uint8_t fade_offset, float sector_light)
{
    int y1 = (int)std::ceil(top - 0.5f);
    int y2 = (int)std::floor(bottom - 0.5f) + 1;
    gli::Sprite* texture = _app->texture_manager().get(texture_id);
    float dvdy = (2.0f * dist) / (_view_distance * _screen_height);
    float texv = (y1 - top) * dvdy;

    if (y1 < _ceiling_mask[x] + 1)
    {
        texv += dvdy * (_ceiling_mask[x] + 1 - y1);
        y1 = _ceiling_mask[x] + 1;
    }

    if (y2 > _floor_mask[x])
    {
        y2 = _floor_mask[x];
    }

    if (texture)
    {
        int dest_stride = _screen_width;
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

    for (int y = vp->min_y; y <= vp->max_y; ++y)
    {
        if (vp->min_x <= vp->max_x)
        {
            int span_start = vp->min_x;
            int span_end = 0;

            float h = vp->height - viewer.h;
            float normy = (float)(_screen_height - 2 * y) / (float)_screen_height;
            float dist = (_view_distance * h) / normy;

            V2f world{};
            V2f forward{ cos_facing, sin_facing };
            V2f right{ sin_facing, -cos_facing };
            world = viewer.p;
            world = world + forward * dist;
            float world_x = world.x;
            float world_y = world.y;

            float pixel_size = 1.0f / (float)_screen_height;
            float pixel_scale = pixel_size * (2.0f * dist) / _view_distance;
            float dudx = pixel_scale * sin_facing;
            float dvdx = pixel_scale * -cos_facing;
            float fade = clamp(1.0f - (dist / _max_fade_dist), 0.0f, 1.0f) * vp->light;

            while (span_start <= vp->max_x)
            {
                for (; span_start <= vp->max_x; ++span_start)
                {
                    if (vp->top[span_start] <= y && vp->bottom[span_start] > y)
                    {
                        break;
                    }
                }

                for (span_end = span_start; span_end <= vp->max_x; ++span_end)
                {
                    if (vp->top[span_end] > y || vp->bottom[span_end] <= y)
                    {
                        break;
                    }
                }

                if (span_end > span_start)
                {
                    if (texture)
                    {
                        float dx = (float)(span_start - _screen_width * 0.5f);
                        float u = world_x + dx * dudx;
                        float v = world_y + dx * dvdx;
                        gli::Pixel* src = texture->pixels();
                        gli::Pixel* dest = _app->get_framebuffer() + (y * _screen_width) + span_start;
                        int iu = (int)std::floor(u * _tex_scale + 0.5f);
                        int iv = (int)std::floor(v * _tex_scale + 0.5f);
                        float ftemp;
                        float uerrstep = std::abs(std::modf(dudx * _tex_scale, &ftemp));
                        int ustep = (int)ftemp;
                        float verrstep = std::abs(std::modf(dvdx * _tex_scale, &ftemp));
                        int vstep = (int)ftemp;
                        float uerror = 0.0f;    //FIXME: Correct initial error
                        float verror = 0.0f;
                        int uerradjust = dudx < 0.0f ? -1 : 1;
                        int verradjust = dvdx < 0.0f ? -1 : 1;

                        for (int x = span_start; x < span_end; ++x)
                        {
                            *dest++ = src[(iu & 63) + (iv & 63) * 64] * fade;
                            iu += ustep;
                            iv += vstep;
                            uerror += uerrstep;
                            verror += verrstep;

                            if (uerror >= 1.0f)
                            {
                                iu += uerradjust;
                                uerror -= 1.0f;
                            }

                            if (verror >= 1.0f)
                            {
                                iv += verradjust;
                                verror -= 1.0f;
                            }
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