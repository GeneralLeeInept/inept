#include "app.h"

#include "types.h"
#include "bsp_tree_builder.h"
#include "wad_loader.h"

#include <algorithm>
#include <set>
#include <fstream>

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

static const size_t None = (size_t)-1;

bool App::on_create()
{
    _config.load();
    return true;
}

void App::on_destroy()
{
    _config.save();
}

bool App::on_update(float delta)
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

Config& App::config()
{
    return _config;
}

int App::world_to_screen_x(float x, float ox)
{
    return (int)std::floor((x - ox) * world_scale) + screen_width() / 2;
}

int App::world_to_screen_y(float y, float oy)
{
    return (int)std::floor((y - oy) * world_scale) + screen_height() / 2;
}

V2i App::world_to_screen(const V2f& p, const V2f& o)
{
    return V2i{ world_to_screen_x(p.x, o.x), world_to_screen_y(p.y, o.y) };
}

V2f App::screen_to_world(const V2i& p, const V2f& o)
{
    V2i pn = p - V2i{ screen_width(), screen_height() } / 2;
    float oo_world_scale = 1.0f / (float)world_scale;
    return V2f{ pn.x * oo_world_scale + o.x, pn.y * oo_world_scale + o.y };
}

void App::save()
{
    std::ofstream fp("res/map.dat", std::ios::binary);

    if (fp)
    {
        uint32_t num_points = (uint32_t)draw_points.size();
        fp.write((char*)&num_points, sizeof(uint32_t));
        fp.write((char*)&draw_points[0], num_points * sizeof(V2f));

        uint32_t num_lines = (uint32_t)draw_lines.size();
        fp.write((char*)&num_lines, sizeof(uint32_t));
        fp.write((char*)&draw_lines[0], num_lines * sizeof(DrawLine));

        fp.write((char*)&spawn_position, sizeof(V2f));
    }
}

void App::load()
{
    std::ifstream fp("res/map.dat", std::ios::binary);

    if (fp)
    {
        uint32_t num_points;
        fp.read((char*)&num_points, sizeof(uint32_t));
        draw_points.resize(num_points);
        fp.read((char*)&draw_points[0], num_points * sizeof(V2f));

        uint32_t num_lines;
        fp.read((char*)&num_lines, sizeof(uint32_t));
        draw_lines.resize(num_lines);
        fp.read((char*)&draw_lines[0], num_lines * sizeof(DrawLine));

        fp.read((char*)&spawn_position, sizeof(V2f));
    }
}

void App::load_bsp_weights(BspTreeBuilder::SplitScoreWeights& weights)
{
    _config.load();
    weights.balance_weight = _config.get("bsp.splitscore.balance_weight", weights.balance_weight);
    weights.split_weight = _config.get("bsp.splitscore.split_weight", weights.split_weight);
    weights.area_ratio_weight = _config.get("bsp.splitscore.area_ratio_weight", weights.area_ratio_weight);
    weights.orthogonal_bonus = _config.get("bsp.splitscore.orthogonal_bonus", weights.orthogonal_bonus);
    _config.save();
}

void App::import()
{
    _config.load();

    const std::string& wad_location = _config.get("import.wadfile", R"(C:\Program Files (x86)\Steam\SteamApps\common\Ultimate Doom\base\DOOM.WAD)");
    std::unique_ptr<WadFile, WadFileDeleter> wadfile(wad_open(wad_location));

    if (!wadfile)
    {
        return;
    }

    const std::string& map_name = _config.get("import.map", "E1M1");
    Wad::Map wadmap;

    if (!wad_load_map(wadfile.get(), map_name.c_str(), wadmap))
    {
        return;
    }

    draw_points.clear();
    draw_lines.clear();
    draw_points.reserve(wadmap.vertices.size());
    draw_lines.reserve(wadmap.linedefs.size());

    // Doom is ~32 units / m
    const float pos_scale = 1.0f / 32.0f;

    for (const Wad::Vertex& dv : wadmap.vertices)
    {
        V2f v = quantize(V2f{ (float)dv.x, (float)dv.y } * pos_scale, 0);
        draw_points.push_back(v);
    }

    for (const Wad::LineDef& ld : wadmap.linedefs)
    {
        add_draw_line(ld.from, ld.to);

        if (ld.sidedefs[1] != 0xffff)
        {
            add_draw_line(ld.to, ld.from);
        }
    }

    for (const Wad::ThingDef& thing : wadmap.things)
    {
        if (thing.type == Wad::ThingType::Player1Start)
        {
            spawn_position = quantize(V2f{ (float)thing.xpos, (float)thing.ypos } * pos_scale, 0);
        }
    }

    _config.save();
}

V2f App::quantize(const V2f& pos, int grid_snap)
{
    if (grid_snap)
    {
        int ix = (int)std::floor(pos.x);
        int iy = (int)std::floor(pos.y);
        ix = (ix / grid_snap) * grid_snap;
        iy = (iy / grid_snap) * grid_snap;
        return V2f{ (float)ix, (float)iy };
    }
    else
    {
        // quantize to 1/1024th meter
        int ix = (int)std::floor(std::abs(pos.x) * 1024.0f);
        int iy = (int)std::floor(std::abs(pos.y) * 1024.0f);

        float qx = (pos.x < 0.0f) ? (ix / -1024.0f) : (ix / 1024.0f);
        float qy = (pos.y < 0.0f) ? (iy / -1024.0f) : (iy / 1024.0f);

        return V2f{ qx, qy };
    }
}

size_t App::select_draw_point(V2i pos_screen)
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

size_t App::add_draw_point(V2i pos_screen, int grid_snap)
{
    size_t existing = select_draw_point(pos_screen);

    if (existing == None)
    {
        existing = draw_points.size();
        V2f pos_world = quantize(screen_to_world(pos_screen, camera_pos), grid_snap);
        draw_points.push_back(pos_world);
    }

    return existing;
}

void App::add_draw_line(size_t from, size_t to)
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

void App::delete_selection()
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

void App::select_rect(const Rectf& rect, bool add)
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
            if (bbox_tlc.x <= draw_points[i].x && draw_points[i].x <= bbox_brc.x && bbox_tlc.y <= draw_points[i].y && draw_points[i].y <= bbox_brc.y)
            {
                editor_data.selection.insert(i);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < draw_points.size(); ++i)
        {
            if (bbox_tlc.x <= draw_points[i].x && draw_points[i].x <= bbox_brc.x && bbox_tlc.y <= draw_points[i].y && draw_points[i].y <= bbox_brc.y)
            {
                editor_data.selection.erase(i);
            }
        }
    }
}

void App::update_editor(float delta)
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
            else if (key_state(gli::Key_I).pressed)
            {
                import();
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
            else if (key_state(gli::Key_P).pressed)
            {
                editor_data.mode = EditorStateData::Mode::PlaceSpawn;
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

    if (key_state(gli::Key_Home).pressed)
    {
        V2f min{ FLT_MAX, FLT_MAX };
        V2f max{ -FLT_MIN, -FLT_MIN };

        for (const V2f& v : draw_points)
        {
            min.x = std::min(v.x, min.x);
            min.y = std::min(v.y, min.y);
            max.x = std::max(v.x, max.x);
            max.y = std::max(v.y, max.y);
        }

        editor_data.camera_pos = (min + max) * 0.5f;
        zoom_level = 8.0f;
        world_scale = std::pow(2.0f, zoom_level);
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
    else if (editor_data.mode == EditorStateData::Mode::DragAdd || editor_data.mode == EditorStateData::Mode::DragRemove)
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
    else if (editor_data.mode == EditorStateData::Mode::Draw)
    {
        if (mouse_state().buttons[gli::MouseButton::Left].pressed)
        {
            bool ctrl = (key_state(gli::Key_LeftControl).down || key_state(gli::Key_RightControl).down);
            bool alt = (key_state(gli::Key_LeftAlt).down || key_state(gli::Key_RightAlt).down);
            int grid_snap = 0;

            if (ctrl)
            {
                grid_snap = 1;
            }
            else if (alt)
            {
                grid_snap = 5;
            }

            editor_data.draw_from = add_draw_point(mouse_pos, grid_snap);
            editor_data.mode = EditorStateData::Mode::DrawLine;
        }
    }
    else if (editor_data.mode == EditorStateData::Mode::DrawLine)
    {
        bool ctrl = (key_state(gli::Key_LeftControl).down || key_state(gli::Key_RightControl).down);
        bool alt = (key_state(gli::Key_LeftAlt).down || key_state(gli::Key_RightAlt).down);
        int grid_snap = 0;

        if (ctrl)
        {
            grid_snap = 1;
        }
        else if (alt)
        {
            grid_snap = 5;
        }

        if (mouse_state().buttons[gli::MouseButton::Right].pressed)
        {
            editor_data.draw_from = None;
            editor_data.mode = EditorStateData::Mode::Draw;
        }
        else if (mouse_state().buttons[gli::MouseButton::Left].pressed)
        {
            size_t to = add_draw_point(mouse_pos, grid_snap);

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
                editor_data.draw_line_to = quantize(screen_to_world(mouse_pos, camera_pos), grid_snap);
            }
            else
            {
                editor_data.draw_line_to = draw_points[existing];
            }

            V2f v = editor_data.draw_line_to - draw_points[editor_data.draw_from];
            editor_data.draw_line_valid = (length_sq(v) > 0.0f);

            if (editor_data.draw_line_valid)
            {
                v = normalize(v);
                editor_data.draw_line_n.x = v.y;
                editor_data.draw_line_n.y = -v.x;
            }
        }
    }
    else if (editor_data.mode == EditorStateData::PlaceSpawn)
    {
        bool ctrl = (key_state(gli::Key_LeftControl).down || key_state(gli::Key_RightControl).down);
        bool alt = (key_state(gli::Key_LeftAlt).down || key_state(gli::Key_RightAlt).down);
        int grid_snap = 0;

        if (ctrl)
        {
            grid_snap = 1;
        }
        else if (alt)
        {
            grid_snap = 5;
        }

        if (mouse_state().buttons[gli::MouseButton::Left].down)
        {
            spawn_position = quantize(screen_to_world(mouse_pos, editor_data.camera_pos), grid_snap);
        }
    }
}

void App::draw_vertex(const V2f& p, gli::Pixel color)
{
    V2i screen_p = world_to_screen(p, camera_pos);
    draw_line(screen_p.x - 2, screen_p.y - 2, screen_p.x + 3, screen_p.y + 3, color);
    draw_line(screen_p.x - 2, screen_p.y + 2, screen_p.x + 3, screen_p.y - 3, color);
}

void App::draw_line_with_normal(const V2f& from, const V2f& to, const V2f& n, gli::Pixel color)
{
    V2i start = world_to_screen(from, camera_pos);
    V2i end = world_to_screen(to, camera_pos);
    V2i mid = (start + end) / 2;
    V2i ntick = mid + V2i{ (int)std::floor(n.x * 10.0f), (int)std::floor(n.y * 10.0f) };
    draw_line(start.x, start.y, end.x, end.y, color);
    draw_line(mid.x, mid.y, ntick.x, ntick.y, color);
}

void App::draw_editor_line(const DrawLine& line, gli::Pixel color)
{
    draw_line_with_normal(draw_points[line.from], draw_points[line.to], line.n, color);
    draw_vertex(draw_points[line.from], color);
    draw_vertex(draw_points[line.to], color);
}

void App::draw_circle(const V2i& p, int r, gli::Pixel color)
{
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    auto plot = [&]() {
        set_pixel(p.x + x, p.y + y, color);
        set_pixel(p.x - x, p.y + y, color);
        set_pixel(p.x + x, p.y - y, color);
        set_pixel(p.x - x, p.y - y, color);
        set_pixel(p.x + y, p.y + x, color);
        set_pixel(p.x - y, p.y + x, color);
        set_pixel(p.x + y, p.y - x, color);
        set_pixel(p.x - y, p.y - x, color);
    };

    plot();

    while (y >= x++)
    {
        if (d > 0)
        {
            y--;
            d = d + 4 * (x - y) + 10;
        }
        else
        {
            d = d + 4 * x + 6;
        }

        plot();
    }
}

void App::render_editor(float delta)
{
    clear_screen(gli::Pixel(80, 80, 80));

    camera_pos = editor_data.camera_pos;

    for (const DrawLine& line : draw_lines)
    {
        draw_editor_line(line, gli::Pixel(0xE2, 0xE2, 0xE2));
    }

    draw_circle(world_to_screen(spawn_position, camera_pos), (int)std::floor(0.5f * world_scale + 0.5f), gli::Pixel(16, 16, 128));

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

void App::update_bsp(float delta)
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
        load_bsp_weights(bsp_data.builder->split_score_weights);

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
        else if (key_state(gli::Key_B).released)
        {
            bsp_data.builder->build();
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

void App::draw_bsp_line(const BspLine& line, gli::Pixel color)
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

void App::clip_line(BspTreeBuilder::Node* node, BspLine& line)
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

void App::draw_bsp_split(BspTreeBuilder::Node* node)
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

void App::draw_bsp_sector(BspTreeBuilder::Sector* sector, bool highlight)
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

void App::draw_bsp_node(BspTreeBuilder::Node* node)
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

void App::render_bsp(float delta)
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

void App::update_simulation(float delta)
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

void App::render_top_down(float delta)
{
    clear_screen(gli::Pixel(0, 128, 128));
}

void App::render_3D(float delta)
{
    clear_screen(gli::Pixel(128, 0, 128));
}

} // namespace fist
