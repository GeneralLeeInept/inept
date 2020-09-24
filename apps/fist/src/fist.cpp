#include <gli.h>

#include "types.h"
#include "bsp_tree_builder.h"

#include <set>
#include <cstdio>

static const float PI = 3.14159274101257324f;

float deg_to_rad(float deg)
{
    static const float conv = PI / 180.0f;
    return deg * conv;
}

float rad_to_deg(float rad)
{
    static const float conv = 180.0f / PI;
    return rad * conv;
}

namespace fist
{

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

    V2f camera_pos{};
    float facing = 0.0f;
    int view = View::Editor;
    int view_state = 0;

    int world_to_screen_x(float x, float ox) { return (int)std::floor((x - ox) * world_scale) + screen_width() / 2; }

    int world_to_screen_y(float y, float oy) { return (int)std::floor((y - oy) * world_scale) + screen_height() / 2; }

    V2i world_to_screen(const V2f& p, const V2f& o) { return V2i{ world_to_screen_x(p.x, o.x), world_to_screen_y(p.y, o.y) }; }

    V2f screen_to_world(const V2i& p, const V2f& o)
    {
        V2i pn = p - V2i{ screen_width(), screen_height() } / 2;
        float oo_world_scale = 1.0f / (float)world_scale;
        return V2f{ pn.x * oo_world_scale + o.x, pn.y * oo_world_scale + o.y };
    }

    void save()
    {
        FILE* fp = fopen(R"(R:\inept\apps\fist\res\map.dat)", "wb");

        if (fp)
        {
            size_t num_points = draw_points.size();
            fwrite(&num_points, sizeof(size_t), 1, fp);

            for (const V2f& p : draw_points)
            {
                fwrite(&p, sizeof(V2f), 1, fp);
            }

            size_t num_lines = draw_lines.size();
            fwrite(&num_lines, sizeof(size_t), 1, fp);

            for (const DrawLine& l : draw_lines)
            {
                fwrite(&l, sizeof(DrawLine), 1, fp);
            }

            fclose(fp);
        }
    }

    void load()
    {
        FILE* fp = fopen(R"(R:\inept\apps\fist\res\map.dat)", "rb");

        if (fp)
        {
            size_t num_points;
            fread(&num_points, sizeof(size_t), 1, fp);

            draw_points.resize(num_points);

            for (V2f& p : draw_points)
            {
                fread(&p, sizeof(V2f), 1, fp);
            }

            size_t num_lines;
            fread(&num_lines, sizeof(size_t), 1, fp);

            draw_lines.resize(num_lines);

            for (DrawLine& l : draw_lines)
            {
                fread(&l, sizeof(DrawLine), 1, fp);
            }

            fclose(fp);
        }
    }

    bool on_create() override { return true; }

    void on_destroy() override {}

    bool on_update(float delta) override
    {
        if (key_state(gli::Key_F5).pressed)
        {
            view = View::Editor;
            view_state = 0;
        }
        else if (key_state(gli::Key_F2).pressed)
        {
            view = View::BSP;
            view_state = 0;
        }
        else if (key_state(gli::Key_F3).pressed)
        {
            view = View::TopDown;
            view_state = 0;
        }
        else if (key_state(gli::Key_F4).pressed)
        {
            view = View::FirstPerson;
            view_state = 0;
        }

        switch (view)
        {
            case View::Editor:
            {
                update_editor(delta);
                render_editor(delta);
                break;
            }
            case View::BSP:
            {
                update_bsp(delta);
                render_bsp(delta);
                break;
            }
            case View::TopDown:
            {
                update_simulation(delta);
                render_top_down(delta);
                break;
            }
            default:
            {
                update_simulation(delta);
                render_3D(delta);
            }
        }

        return true;
    }

    float zoom_level = 4.0f;
    float world_scale = 16.0f; // Pixels per meter in top down view
    const size_t None = (size_t)-1;

    std::vector<V2f> draw_points;

    struct DrawLine
    {
        V2f n;
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
            DrawLine
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

    size_t select_draw_point(V2i pos_screen)
    {
        V2f pos_world = screen_to_world(pos_screen, camera_pos);
        float search_radius = 5.0f / world_scale;
        float search_radius_sq = (search_radius * search_radius);
        float nearest = FLT_MAX;
        size_t found = None;

        for (size_t i = 0; i < draw_points.size(); ++i)
        {
            float dist_sq = length_sq(pos_world - draw_points[i]);
            if (dist_sq < search_radius_sq && dist_sq < nearest)
            {
                nearest = dist_sq;
                found = i;
            }
        }

        return found;
    }

    size_t add_draw_point(V2i pos_screen)
    {
        size_t existing = select_draw_point(pos_screen);

        if (existing == None)
        {
            existing = draw_points.size();
            V2f pos_world = screen_to_world(pos_screen, camera_pos);

            // Quantize to cm
            int qx = (int)std::floor(pos_world.x * 100.0f);
            int qy = (int)std::floor(pos_world.y * 100.0f);

            pos_world.x = qx / 100.0f;
            pos_world.y = qy / 100.0f;

            draw_points.push_back(pos_world);
        }

        return existing;
    }

    void add_draw_line(size_t from, size_t to)
    {
        for (DrawLine& line : draw_lines)
        {
            if (line.from == from && line.to == to)
            {
                return;
            }
        }

        DrawLine line{};
        line.from = from;
        line.to = to;

        V2f v = normalize(draw_points[to] - draw_points[from]);
        line.n.x = v.y;
        line.n.y = -v.x;

        draw_lines.push_back(line);
    }

    void delete_selection()
    {
        auto& line_iter = draw_lines.begin();

        while (line_iter != draw_lines.end())
        {
            if (editor_data.selection.find(line_iter->from) != editor_data.selection.end())
            {
                line_iter = draw_lines.erase(line_iter);
            }
            else if (editor_data.selection.find(line_iter->to) != editor_data.selection.end())
            {
                line_iter = draw_lines.erase(line_iter);
            }
            else
            {
                ++line_iter;
            }
        }

        auto& vert_iter = editor_data.selection.rbegin();

        while (vert_iter != editor_data.selection.rend())
        {
            // Remove the vertex and fix references(!!!)
            size_t sentinel = *vert_iter;
            draw_points.erase(draw_points.begin() + sentinel);

            for (auto& line : draw_lines)
            {
                if (line.from > sentinel) line.from--;
                if (line.to > sentinel) line.to--;
            }

            ++vert_iter;
        }

        editor_data.selection.clear();
    }

    void select_rect(const Rectf& rect, bool add)
    {
        // FIXME - editor selection rect is a bounding box really
        V2f bbox_tlc = rect.origin;
        V2f bbox_brc = rect.extents;

        if (bbox_tlc.x > bbox_brc.x)
        {
            std::swap(bbox_tlc.x, bbox_brc.x);
        }

        if (bbox_tlc.y > bbox_brc.y)
        {
            std::swap(bbox_tlc.y, bbox_brc.y);
        }

        if (add)
        {
            for (size_t i = 0; i < draw_points.size(); ++i)
            {
                if (bbox_tlc.x <= draw_points[i].x && draw_points[i].x <= bbox_brc.x && bbox_tlc.y <= draw_points[i].y &&
                    draw_points[i].y <= bbox_brc.y)
                {
                    editor_data.selection.insert(i);
                }
            }
        }
        else
        {
            for (size_t i = 0; i < draw_points.size(); ++i)
            {
                if (bbox_tlc.x <= draw_points[i].x && draw_points[i].x <= bbox_brc.x && bbox_tlc.y <= draw_points[i].y &&
                    draw_points[i].y <= bbox_brc.y)
                {
                    editor_data.selection.erase(i);
                }
            }
        }
    }

    void update_editor(float delta)
    {
        if (key_state(gli::Key_Escape).pressed)
        {
            view_state = 0;
        }

        if (view_state == 0)
        {
            editor_data.selection.clear();
            editor_data.mode = EditorStateData::Mode::Select;
            editor_data.draw_from = None;
            view_state = 1;
        }

        editor_data.draw_line_valid = false;

        bool lmb = mouse_state().buttons[gli::MouseButton::Left].down;
        bool mmb = mouse_state().buttons[gli::MouseButton::Middle].down;
        bool rmb = mouse_state().buttons[gli::MouseButton::Right].down;
        V2i mouse_pos{ mouse_state().x, mouse_state().y };

        if (!(lmb || mmb || rmb))
        {
            if (key_state(gli::Key_LeftControl).down || key_state(gli::Key_RightControl).down)
            {
                if (key_state(gli::Key_R).pressed)
                {
                    draw_points.clear();
                    draw_lines.clear();
                    view_state = 0;
                    return;
                }
                else if (key_state(gli::Key_S).pressed)
                {
                    save();
                }
                else if (key_state(gli::Key_L).pressed)
                {
                    load();
                    view_state = 0;
                    return;
                }
            }
            else
            {
                if (key_state(gli::Key_S).pressed)
                {
                    editor_data.mode = EditorStateData::Mode::Select;
                }
                else if (key_state(gli::Key_M).pressed)
                {
                    editor_data.mode = EditorStateData::Mode::Move;
                }
                else if (key_state(gli::Key_D).pressed)
                {
                    editor_data.mode = EditorStateData::Mode::Draw;
                }
            }
        }

        if (mouse_state().buttons[gli::MouseButton::Middle].pressed)
        {
            editor_data.mouse_anchor_pos = mouse_pos;
        }
        else if (mouse_state().buttons[gli::MouseButton::Middle].down)
        {
            V2i mouse_move = editor_data.mouse_anchor_pos - mouse_pos;
            V2f mouse_movef{ (float)mouse_move.x, (float)mouse_move.y };
            editor_data.camera_pos = editor_data.camera_pos + (mouse_movef / world_scale);
            editor_data.mouse_anchor_pos = mouse_pos;
        }

        if (mouse_state().wheel)
        {
            if (mouse_state().wheel < 0 && zoom_level > 0.0f)
            {
                zoom_level -= 0.5f;
            }
            else if (mouse_state().wheel > 0 && zoom_level < 8.0f)
            {
                zoom_level += 0.5f;
            }

            V2f prev_camera_pos = screen_to_world(mouse_pos, V2f{});
            world_scale = std::pow(2.0f, zoom_level);
            V2f new_camera_pos = screen_to_world(mouse_pos, V2f{});
            editor_data.camera_pos = editor_data.camera_pos + prev_camera_pos - new_camera_pos;
        }

        if (editor_data.mode == EditorStateData::Mode::Select)
        {
            bool add = (key_state(gli::Key_LeftControl).down || key_state(gli::Key_RightControl).down);
            bool remove = (key_state(gli::Key_LeftAlt).down || key_state(gli::Key_RightAlt).down);

            if (mouse_state().buttons[gli::MouseButton::Left].pressed)
            {
                editor_data.saved_selection = editor_data.selection;

                if (remove)
                {
                    size_t sel = select_draw_point(mouse_pos);

                    if (sel != None)
                    {
                        editor_data.selection.erase(sel);
                    }

                    editor_data.mode = EditorStateData::Mode::DragRemove;
                    editor_data.drag_rect.origin = screen_to_world(mouse_pos, editor_data.camera_pos);
                }
                else
                {
                    if (!add)
                    {
                        editor_data.selection.clear();
                    }


                    size_t sel = select_draw_point(mouse_pos);

                    if (sel != None)
                    {
                        editor_data.selection.insert(sel);
                    }

                    editor_data.mode = EditorStateData::Mode::DragAdd;
                    editor_data.drag_rect.origin = screen_to_world(mouse_pos, editor_data.camera_pos);
                }
            }
            else if (key_state(gli::Key_Delete).pressed)
            {
                delete_selection();
            }
        }

        if (editor_data.mode == EditorStateData::Mode::DragAdd || editor_data.mode == EditorStateData::Mode::DragRemove)
        {
            editor_data.drag_rect.extents = screen_to_world(mouse_pos, editor_data.camera_pos);

            if (!mouse_state().buttons[gli::MouseButton::Left].down)
            {
                select_rect(editor_data.drag_rect, (editor_data.mode == EditorStateData::Mode::DragAdd));
                editor_data.mode = EditorStateData::Mode::Select;
            }
            else
            {
                if (mouse_state().buttons[gli::MouseButton::Right].pressed)
                {
                    editor_data.selection = std::move(editor_data.saved_selection);
                    editor_data.mode = EditorStateData::Mode::Select;
                }
            }
        }

        if (editor_data.mode == EditorStateData::Mode::Draw)
        {
            if (mouse_state().buttons[gli::MouseButton::Left].pressed)
            {
                editor_data.draw_from = add_draw_point(mouse_pos);
                editor_data.mode = EditorStateData::Mode::DrawLine;
            }
        }

        if (editor_data.mode == EditorStateData::Mode::DrawLine)
        {
            if (mouse_state().buttons[gli::MouseButton::Right].pressed)
            {
                editor_data.draw_from = None;
                editor_data.mode = EditorStateData::Mode::Draw;
            }
            else if (mouse_state().buttons[gli::MouseButton::Left].pressed)
            {
                size_t to = add_draw_point(mouse_pos);

                if (to != editor_data.draw_from)
                {
                    add_draw_line(editor_data.draw_from, to);
                    editor_data.draw_from = to;
                }
            }
            else
            {
                size_t existing = select_draw_point(mouse_pos);

                if (existing == None)
                {
                    V2f pos_world = screen_to_world(mouse_pos, camera_pos);

                    // Quantize to cm
                    int qx = (int)std::floor(pos_world.x * 100.0f);
                    int qy = (int)std::floor(pos_world.y * 100.0f);

                    editor_data.draw_line_to.x = qx / 100.0f;
                    editor_data.draw_line_to.y = qy / 100.0f;
                    editor_data.draw_line_valid = true;
                }
                else if (existing != editor_data.draw_from)
                {
                    editor_data.draw_line_valid = true;
                    editor_data.draw_line_to = draw_points[existing];
                }

                if (editor_data.draw_line_valid)
                {
                    V2f v = normalize(editor_data.draw_line_to - draw_points[editor_data.draw_from]);
                    editor_data.draw_line_n.x = v.y;
                    editor_data.draw_line_n.y = -v.x;
                }
            }
        }
    }

    void draw_vertex(const V2f& p, gli::Pixel color)
    {
        V2i screen_p = world_to_screen(p, camera_pos);
        draw_line(screen_p.x - 2, screen_p.y - 2, screen_p.x + 3, screen_p.y + 3, color);
        draw_line(screen_p.x - 2, screen_p.y + 2, screen_p.x + 3, screen_p.y - 3, color);
    }

    void draw_line_with_normal(const V2f& from, const V2f& to, const V2f& n, gli::Pixel color)
    {
        V2i start = world_to_screen(from, camera_pos);
        V2i end = world_to_screen(to, camera_pos);
        V2i mid = (start + end) / 2;
        V2i ntick = mid + V2i{ (int)std::floor(n.x * 10.0f), (int)std::floor(n.y * 10.0f) };
        draw_line(start.x, start.y, end.x, end.y, color);
        draw_line(mid.x, mid.y, ntick.x, ntick.y, color);
    }

    void draw_editor_line(const DrawLine& line, gli::Pixel color)
    {
        draw_line_with_normal(draw_points[line.from], draw_points[line.to], line.n, color);
        draw_vertex(draw_points[line.from], color);
        draw_vertex(draw_points[line.to], color);
    }

    void render_editor(float delta)
    {
        clear_screen(gli::Pixel(80, 80, 80));

        camera_pos = editor_data.camera_pos;

        for (const DrawLine& line : draw_lines)
        {
            draw_editor_line(line, gli::Pixel(0xE2, 0xE2, 0xE2));
        }

        if (editor_data.mode == EditorStateData::Mode::Select || editor_data.mode == EditorStateData::Mode::DragAdd ||
            editor_data.mode == EditorStateData::Mode::DragRemove || editor_data.mode == EditorStateData::Mode::Move)
        {
            for (size_t sel : editor_data.selection)
            {
                draw_vertex(draw_points[sel], gli::Pixel(255, 106, 0));
            }
        }
        if (editor_data.mode == EditorStateData::Mode::DragAdd || editor_data.mode == EditorStateData::Mode::DragRemove)
        {
            set_blend_mode(gli::BlendMode::One, gli::BlendMode::One, 0);
            set_blend_op(gli::BlendOp::Add);
            V2i screen_corner1 = world_to_screen(editor_data.drag_rect.origin, camera_pos);
            V2i screen_corner2 = world_to_screen(editor_data.drag_rect.extents, camera_pos);

            if (screen_corner1.x > screen_corner2.x)
            {
                std::swap(screen_corner1.x, screen_corner2.x);
            }

            if (screen_corner1.y > screen_corner2.y)
            {
                std::swap(screen_corner1.y, screen_corner2.y);
            }

            V2i screen_extent = screen_corner2 - screen_corner1;
            fill_rect(screen_corner1.x, screen_corner1.y, screen_extent.x, screen_extent.y, 0, gli::Pixel(128, 128, 0), 0);
            set_blend_op(gli::BlendOp::None);
        }
        if (editor_data.mode == EditorStateData::Mode::Draw || editor_data.mode == EditorStateData::Mode::DrawLine)
        {
            if (editor_data.draw_line_valid)
            {
                V2f from = draw_points[editor_data.draw_from];
                draw_line_with_normal(from, editor_data.draw_line_to, editor_data.draw_line_n, gli::Pixel(255, 106, 0));
            }
        }
    }

    struct BspStateData
    {
        std::unique_ptr<BspTreeBuilder> builder;
        BspTreeBuilder::Sector* current_sector;
    } bsp_data;

    void update_bsp(float delta)
    {
        if (key_state(gli::Key_R).pressed)
        {
            view_state = 0;
        }

        if (view_state == 0)
        {
            bsp_data.builder = std::make_unique<BspTreeBuilder>();
            bsp_data.current_sector = nullptr;

            std::vector<BspLine> bsp_lines;

            for (const DrawLine& draw_line : draw_lines)
            {
                BspLine line;
                line.a = draw_points[draw_line.from];
                line.b = draw_points[draw_line.to];
                line.n = draw_line.n;
                bsp_lines.push_back(line);
            }

            bsp_data.builder->init(bsp_lines);

            view_state = 1;
        }

        V2i mouse_pos{ mouse_state().x, mouse_state().y };

        if (mouse_state().buttons[gli::MouseButton::Middle].pressed)
        {
            editor_data.mouse_anchor_pos = mouse_pos;
        }
        else if (mouse_state().buttons[gli::MouseButton::Middle].down)
        {
            V2i mouse_move = editor_data.mouse_anchor_pos - mouse_pos;
            V2f mouse_movef{ (float)mouse_move.x, (float)mouse_move.y };
            editor_data.camera_pos = editor_data.camera_pos + (mouse_movef / world_scale);
            editor_data.mouse_anchor_pos = mouse_pos;
        }

        if (mouse_state().wheel < 0 && zoom_level > 0)
        {
            --zoom_level;
        }
        else if (mouse_state().wheel > 0 && zoom_level < 8)
        {
            ++zoom_level;
        }

        world_scale = std::pow(2.0f, (float)zoom_level);
        if (view_state == 1)
        {
            if (key_state(gli::Key_Space).released)
            {
                bsp_data.builder->split();
            }

            if (bsp_data.builder->complete())
            {
                view_state = 2;
            }
        }

        if (view_state == 2)
        {
            V2f mouse_world_pos = screen_to_world(V2i{ mouse_state().x, mouse_state().y }, editor_data.camera_pos);
            BspTreeBuilder::Node* node = &bsp_data.builder->root;

            while (node && !node->sector)
            {
                int side = BspLine::side(node->split, mouse_world_pos);

                if (side >= 0)
                {
                    node = node->front.get();
                }
                else
                {
                    node = node->back.get();
                }
            }

            bsp_data.current_sector = node ? node->sector.get() : nullptr;
        }
    }

    void draw_bsp_line(const BspLine& line, gli::Pixel color)
    {
        V2i from = world_to_screen(line.a, camera_pos);
        V2i to = world_to_screen(line.b, camera_pos);
        V2i mid = (from + to) / 2;
        V2i ntick = mid + V2i{ (int)std::floor(line.n.x * 10.0f), (int)std::floor(line.n.y * 10.0f) };
        draw_line(from.x, from.y, to.x, to.y, color);
        draw_line(mid.x, mid.y, ntick.x, ntick.y, color);
        draw_line(from.x - 2, from.y - 2, from.x + 3, from.y + 3, color);
        draw_line(from.x - 2, from.y + 2, from.x + 3, from.y - 3, color);
    }

    void clip_line(BspTreeBuilder::Node* node, BspLine& line)
    {
        while (node->parent && length_sq(line.b - line.a) > 0.0f)
        {
            BspLine front;
            BspLine back;

            if (BspLine::split(node->parent->split, line, front, back))
            {
                line = (node->parent->front.get() == node) ? front : back;
            }

            node = node->parent;
        }
    }

    void draw_bsp_split(BspTreeBuilder::Node* node)
    {
        BspLine line = node->split;
        V2f v = normalize(line.b - line.a);
        V2f o = line.a;
        line.a = o - v * 10000.0f;
        line.b = o + v * 10000.0f;
        clip_line(node, line);

        BspLine discard;
        BspLine screen_edge;

        screen_edge.a = screen_to_world(V2i{}, camera_pos);
        screen_edge.n = V2f{ 1.0f, 0.0f };
        BspLine::split(screen_edge, line, line, discard);

        screen_edge.n = V2f{ 0.0f, 1.0f };
        BspLine::split(screen_edge, line, line, discard);

        screen_edge.a = screen_to_world(V2i{ screen_width(), screen_height() }, camera_pos);
        screen_edge.n = V2f{ -1.0f, 0.0f };
        BspLine::split(screen_edge, line, line, discard);

        screen_edge.n = V2f{ 0.0f, -1.0f };
        BspLine::split(screen_edge, line, line, discard);

        draw_bsp_line(line, gli::Pixel(128, 128, 128));
    }

    void draw_bsp_sector(BspTreeBuilder::Sector* sector, bool highlight)
    {
        gli::Pixel color = sector->convex() ? gli::Pixel(64, 128, 64) : gli::Pixel(128, 64, 64);

        if (highlight)
        {
            color = gli::Pixel(255, 255, 0);
        }

        for (const BspLine& line : sector->lines)
        {
            draw_bsp_line(line, color);
        }
    }

    void draw_bsp_node(BspTreeBuilder::Node* node)
    {
        if (node->sector)
        {
            draw_bsp_sector(node->sector.get(), false);
        }
        else
        {
            draw_bsp_node(node->front.get());
            draw_bsp_node(node->back.get());

            set_blend_mode(gli::BlendMode::One, gli::BlendMode::One, 0);
            set_blend_op(gli::BlendOp::Add);
            draw_bsp_split(node);
            set_blend_op(gli::BlendOp::None);
        }
    }

    void render_bsp(float delta)
    {
        clear_screen(gli::Pixel(0, 0, 32));

        camera_pos = editor_data.camera_pos;

        for (const DrawLine& line : draw_lines)
        {
            draw_editor_line(line, gli::Pixel(16, 16, 32));
        }

        draw_bsp_node(&bsp_data.builder->root);

        if (bsp_data.current_sector)
        {
            draw_bsp_sector(bsp_data.current_sector, true);
        }
    }

    void update_simulation(float delta)
    {
        const float move_speed = 1.4f;
        const float turn_speed = PI;
        V2f move{};
        float delta_facing = 0.0f;

        if (key_state(gli::Key_W).down)
        {
            move.x += 1.0f;
        }

        if (key_state(gli::Key_S).down)
        {
            move.x -= 1.0f;
        }

        if (key_state(gli::Key_A).down)
        {
            move.y -= 1.0f;
        }

        if (key_state(gli::Key_D).down)
        {
            move.y += 1.0f;
        }

        if (key_state(gli::Key_Left).down)
        {
            delta_facing -= 1.0f;
        }

        if (key_state(gli::Key_Right).down)
        {
            delta_facing += 1.0f;
        }

        facing = facing + delta_facing * turn_speed * delta;

        while (facing < 0.0f)
        {
            facing += 2.0f * PI;
        }

        while (facing > 2.0f * PI)
        {
            facing -= 2.0f * PI;
        }

        V2f forward{ std::cos(facing), std::sin(facing) };
        V2f right{ -std::sin(facing), std::cos(facing) };
        move = (forward * move.x * move_speed * delta) + (right * move.y * move_speed * delta);
        camera_pos = camera_pos + move;
    }

    void render_top_down(float delta) { clear_screen(gli::Pixel(0, 128, 128)); }

    void render_3D(float delta) { clear_screen(gli::Pixel(128, 0, 128)); }
};

} // namespace fist

fist::App app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Fist", 1280, 720, 1))
    {
        app.show_mouse(true);
        app.run();
    }

    return 0;
}
