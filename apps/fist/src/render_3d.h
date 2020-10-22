#pragma once

#include "types.h"

#include <memory>

namespace fist
{

class App;
struct LineSeg;
struct Map;

class Render3D
{
public:
    void init(App* app);

    void reload_config();
    void draw_3d(const ThingPos& viewer, Map* map);

private:
    struct VisPlane
    {
        uint64_t texture_id;
        float height;
        float light;
        int min_y, max_y;
        int min_x, max_x;
        std::unique_ptr<int[]> top;
        std::unique_ptr<int[]> bottom;
    };

    struct SolidColumns
    {
        int min;
        int max;
        SolidColumns* next;
    };

    bool _config_dirty{};
    float _fov_y{};
    float _screen_aspect{};
    float _view_distance{};
    float _max_fade_dist{};
    float _tex_scale{};
    float _near_clip{};

    App* _app{};
    Map* _map{};
    int _screen_width{};
    int _screen_height{};
    Transform2D _world_view{};
    std::unique_ptr<int[]> _floor_height{};
    std::unique_ptr<int[]> _ceiling_height{};
    std::unique_ptr<SolidColumns[]> _solid_columns{};
    int _num_solid_columns{};
    std::unique_ptr<VisPlane[]> _visplanes{};
    size_t _max_visplanes{};
    size_t _num_visplanes{};
    size_t _ceiling_plane;
    size_t _floor_plane;

    void load_configs();
    void grow_visplanes();
    size_t find_visplane(uint64_t texture, float height, float light);
    size_t check_visplane(size_t plane, int x_start, int x_end);

    void draw_node(const ThingPos& viewer, uint32_t index);
    bool cull_node(const ThingPos& viewer, uint32_t index, int child);
    void draw_subsector(const ThingPos& viewer, uint32_t index);
    void draw_line(const ThingPos& viewer, const LineSeg* lineseg);
    void draw_column(int x, float dist, float texu, float top, float bottom, uint64_t texture_id, uint8_t fade_offset, float sector_light);
    void draw_plane(const ThingPos& viewer, size_t plane);
};

}