#pragma once

#include <gli_core.h>

#include "types.h"
#include "bsp_tree_builder.h"
#include "config.h"

#include <deque>
#include <set>
#include <string>

namespace fist
{

class AppState;

class App : public gli::App
{
public:
    enum View
    {
        Editor,
        BSP,
        TopDown,
        FirstPerson,
        Count
    };

    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;

    Config& config();

    // Editor / BSP?
    int world_to_screen_x(float x, float ox);
    int world_to_screen_y(float y, float oy);
    V2i world_to_screen(const V2f& p, const V2f& o);
    V2f screen_to_world(const V2i& p, const V2f& o);

    // Editor
    void save();
    void load();

private:
    using StateStack = std::deque<AppState*>;
    StateStack _app_states;

    Config _config{};

    V2f camera_pos{};
    float facing = 0.0f;
    int view = View::Editor;
    int view_state = 0;

    // BSP Builder
    void load_bsp_weights(BspTreeBuilder::SplitScoreWeights& weights);

    // Editor
    void import();

    // Editor/BSP (TODO: Camera controller)
    float zoom_level = 4.0f;
    float world_scale = 16.0f; // Pixels per meter in top down view

    // Editor
    std::vector<V2f> draw_points;
    V2f spawn_position{};

    struct DrawLine
    {
        V2f normal;
        size_t from;
        size_t to;
    };

    std::vector<DrawLine> draw_lines;

    struct EditorStateData
    {
        enum Mode
        {
            Select,
            DragAdd,
            DragRemove,
            Move,
            Draw,
            DrawLine,
            PlaceSpawn
        };

        using Selection = std::set<size_t>;
        Selection selection{};
        Selection saved_selection{};

        Mode mode{ Mode::Select };

        size_t draw_from;
        V2f draw_line_to;
        V2f draw_line_n;
        bool draw_line_valid;
        V2f camera_pos{};
        V2i mouse_anchor_pos{};
        Rectf drag_rect;
    } editor_data;

    // Editor
    V2f quantize(const V2f& pos, int grid_snap);
    size_t select_draw_point(V2i pos_screen);
    size_t add_draw_point(V2i pos_screen, int grid_snap);
    void add_draw_line(size_t from, size_t to);
    void delete_selection();
    void select_rect(const Rectf& rect, bool add);
    void update_editor(float delta);

    // Editor / BSP
    void draw_vertex(const V2f& p, gli::Pixel color);
    void draw_line_with_normal(const V2f& from, const V2f& to, const V2f& n, gli::Pixel color);

    // Editor
    void draw_editor_line(const DrawLine& line, gli::Pixel color);
    void draw_circle(const V2i& p, int r, gli::Pixel color);
    void render_editor(float delta);

    // BSP
    struct BspStateData
    {
        std::unique_ptr<BspTreeBuilder> builder;
        BspTreeBuilder::Sector* current_sector;
    } bsp_data;

    void update_bsp(float delta);
    void draw_bsp_line(const BspLine& line, gli::Pixel color);
    void clip_line(BspTreeBuilder::Node* node, BspLine& line);
    void draw_bsp_split(BspTreeBuilder::Node* node);
    void draw_bsp_sector(BspTreeBuilder::Sector* sector, bool highlight);
    void draw_bsp_node(BspTreeBuilder::Node* node);
    void render_bsp(float delta);

    // Play
    void update_simulation(float delta);
    void render_top_down(float delta);
    void draw_line_3d(V2f from, V2f to);
    void draw_bsp_sector_view(BspTreeBuilder::Sector* sector);
    void draw_bsp_node_view(BspTreeBuilder::Node* node);
    void render_3D(float delta);
};

}
