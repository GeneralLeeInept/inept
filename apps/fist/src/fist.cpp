#include <gli.h>

#include "types.h"

#include <deque>

// clang-format off
static const char* TestMap = 
"################################################################"
"#....*.......................#.................................#"
"###########################.##.................................#"
"#..........#.................#.................................#"
"#..........#.................#.................................#"
"#..........#.................#.................................#"
"#....*.....#.................#.................................#"
"#..........#########.#########.................................#"
"#..........#.................#.................................#"
"#.......... .................#.................................#"
"#..........#.................#.................................#"
"#..........############...####.................................#"
"############.................#.................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#.........................*....................................#"
"#..............................................................#"
"#.........###################################..................#"
"#.........#.................................#..................#"
"#.........#..#...#...#...#...#...#...#...#..#..................#"
"#.........#.................................#..................#"
"#.........#....#...#...#...#...#...#...#. ..#..................#"
"#.........#................@................#..................#"
"#.........#..#...#...#...#...#...#...#...#..#..................#"
"#.........#.................................#..................#"
"#.........#....#...#...#.*.#...#...#...#. ..#..................#"
"#.........#.................................#..................#"
"#.........#..#...#...#...#...#...#...#...#..#..................#"
"#.........#.................................#..................#"
"#.........#....#...#...#...#...#...#...#. ..#..................#"
"#.........#.................................#..................#"
"#.........#..#...#...#...#...#...#...#...#..#..................#"
"#.........###################################..................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"#..............................................................#"
"################################################################";

static const int MapWidth = 64;
static const int MapHeight = 64;
static const int FixedScale = 64;


//static const char* BspMap =
//        "#......#"
//        "##.....#"
//        "#..#...#"
//        "#.###..#"
//        "#..#...#"
//        "#....###"
//        "#..#...#"
//        "########";

//static const char* BspMap =
//        "########"
//        "#......#"
//        "#......#"
//        "#......#"
//        "#......#"
//        "#......#"
//        "#......#"
//        "########";
//
//static const char* BspMap =
//        "########"
//        "#......#"
//        "#......#"
//        "#..#...#"
//        "#......#"
//        "#....#.#"
//        "#......#"
//        "########";
//
//static const int BspMapWidth = 8;
//static const int BspMapHeight = 8;
//static const char* BspMap = TestMap;
//static const int BspMapWidth = MapWidth;
//static const int BspMapHeight = MapHeight;

static const char* BspMap =
        "################"
        "#..............#"
        "#....#...#.....#"
        "#..#........####"
        "#......#....#..#"
        "#....#...#.....#"
        "#...........#..#"
        "################";

static const int BspMapWidth = 16;
static const int BspMapHeight = 8;

static const float PI = 3.14159274101257324f;

char readmap(int x, int y)
{
    if (x >= 0 && x < MapWidth && y >= 0 && y < MapHeight)
    {
        int i = y * MapWidth + x;
        return TestMap[i];
    }

    return '#'; // Everything outside the map is solid
}

bool solid(int x, int y)
{
    return readmap(x, y) == '#';
}

bool solid(const V2f& pos)
{
    return solid((int)std::floor(pos.x), (int)std::floor(pos.y));
}

void real_to_coarse_and_fine(const V2f& real_pos, V2i& coarse, V2i& fine)
{
    V2f real_coarse{ std::floor(real_pos.x), std::floor(real_pos.y) };
    V2f real_fine = real_pos - real_coarse * (float)FixedScale;
    coarse.x = (int)real_coarse.x;
    coarse.y = (int)real_coarse.y;
    fine.x = (int)(std::floor(real_fine.x) + 0.5f);
    fine.y = (int)(std::floor(real_fine.y) + 0.5f);
}

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
// clang-format on

class Fist : public gli::App
{
public:
    enum View
    {
        BSP,
        TopDown,
        FirstPerson,
        Count
    };

    V2f camera_pos{};
    float facing = 0.0f;
    int view = View::BSP;

    struct Light
    {
        V2f position;
        gli::Pixel color;
        float intensity;
    };

    std::vector<Light> lights{};

    bool on_create() override
    {
        for (int x = 0; x < MapWidth; ++x)
        {
            for (int y = 0; y < MapHeight; ++y)
            {
                char c = readmap(x, y);
                if (c == '@')
                {
                    camera_pos.x = (float)x + 0.5f;
                    camera_pos.y = (float)y + 0.5f;
                }
                else if (c == '*')
                {
                    Light l;
                    l.position.x = (float)x + 0.5f;
                    l.position.y = (float)y + 0.5f;
                    l.color = gli::Pixel(255, 255, 255);
                    l.intensity = 100.0f;
                    lights.push_back(l);
                }
            }
        }

        return true;
    }

    void on_destroy() override {}

    bool on_update(float delta) override
    {
        if (key_state(gli::Key_F2).pressed)
        {
            view = View::BSP;
            bsp_state = BspState::Reset;
        }
        else if (key_state(gli::Key_F3).pressed)
        {
            view = View::TopDown;
        }
        else if (key_state(gli::Key_F4).pressed)
        {
            view = View::FirstPerson;
        }

        switch (view)
        {
            case View::BSP:
            {
                update_bsp(delta);
                render_bsp(delta);
                break;
            }
            case View::TopDown:
            {
                update(delta);
                render_top_down(delta);
                break;
            }
            default:
            {
                update(delta);
                render_3D(delta);
            }
        }

        return true;
    }

    void update(float delta)
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

    void render_top_down(float delta)
    {
        int tile_scale = 32;
        auto world_to_screen_x = [&](float x) -> int { return (int)std::floor((x - camera_pos.x) * tile_scale) + screen_width() / 2; };
        auto world_to_screen_y = [&](float y) -> int { return (int)std::floor((y - camera_pos.y) * tile_scale) + screen_height() / 2; };
        auto world_to_screen = [&](const V2f& p) -> V2i { return V2i{ world_to_screen_x(p.x), world_to_screen_y(p.y) }; };

        clear_screen(gli::Pixel(0xFF404040));

        for (int y = 0; y < MapHeight; ++y)
        {
            int ty = world_to_screen_y((float)y);

            for (int x = 0; x < MapWidth; ++x)
            {
                int tx = world_to_screen_x((float)x);

                if (solid(x, y))
                {
                    fill_rect(tx, ty, tile_scale, tile_scale, 0, gli::Pixel(0xFF000080), gli::Pixel(0xFFE2E2E2));
                }
                else
                {
                    fill_rect(tx, ty, tile_scale, tile_scale, 1, gli::Pixel(0xFF000000), gli::Pixel(0xFFE2E2E2));
                }
            }
        }

        for (const Light& light : lights)
        {
            V2i light_screen_pos = world_to_screen(light.position);
            float angle = 0.0f;
            float angle_step = deg_to_rad(1.0f);

            for (int i = 0; i < 360; ++i, angle += angle_step)
            {
                float cos_angle = std::cos(angle);
                float sin_angle = std::sin(angle);
                V2f direction{ cos_angle, sin_angle };
                V2f hit_pos{};
                V2f hit_normal{};
                int hit_tile_id;
                cast_ray(light.position, direction, hit_pos, hit_normal, hit_tile_id);
                V2i screen_hit_pos = world_to_screen(hit_pos);
                draw_line(light_screen_pos.x, light_screen_pos.y, screen_hit_pos.x, screen_hit_pos.y, light.color);
            }
        }

        V2i camera_screen_pos = world_to_screen(camera_pos);
#if 0
        int col_x = -screen_width() / 2;
        static const float screen_dist = screen_width() * 0.5f;
        float cos_facing = std::cos(facing);
        float sin_facing = std::sin(facing);

        for (int i = 0; i < screen_width(); ++i)
        {
            float angle = std::atan2(screen_dist, (float)col_x);
            float cos_angle = std::cos(angle);
            float sin_angle = std::sin(angle);
            V2f direction{ cos_facing * sin_angle - sin_facing * cos_angle, sin_facing * sin_angle + cos_facing * cos_angle };
            V2f hit_pos{};
            V2f hit_normal{};
            int hit_tile_id;
            cast_ray(camera_pos, direction, hit_pos, hit_normal, hit_tile_id);

            V2i screen_hit_pos = world_to_screen(hit_pos);
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;

            if (col_x < 0)
            {
                float t = (float)col_x / (-screen_width() * 0.5f);
                r = (uint8_t)std::floor(255.0f * t);
                g = (uint8_t)std::floor(255.0f * (1.0f - t));
            }
            else
            {
                float t = (float)col_x / (screen_width() * 0.5f);
                g = (uint8_t)std::floor(255.0f * (1.0f - t));
                b = (uint8_t)std::floor(255.0f * t);
            }

            draw_line(camera_screen_pos.x, camera_screen_pos.y, screen_hit_pos.x, screen_hit_pos.y, gli::Pixel(r, g, b));
            col_x++;
        }
#else
        float cos_facing = std::cos(facing);
        float sin_facing = std::sin(facing);
        V2f direction = V2f{ cos_facing, sin_facing } * 10.0f;
        draw_line(camera_screen_pos.x, camera_screen_pos.y, (int)std::floor(camera_screen_pos.x + direction.x),
                  (int)std::floor(camera_screen_pos.y + direction.y), gli::Pixel(0xFFC0C000));
#endif

        fill_rect(camera_screen_pos.x - 4, camera_screen_pos.y - 4, 8, 8, 0, gli::Pixel(0xFFC0C000), 0);
    }

    void render_3D(float delta)
    {
        int col_x = -screen_width() / 2;
        static const float screen_dist = screen_width() * 0.5f;
        float cos_facing = std::cos(facing);
        float sin_facing = std::sin(facing);

        clear_screen(gli::Pixel(0));

        for (int i = 0; i < screen_width(); ++i)
        {
            float angle = std::atan2(screen_dist, (float)col_x);
            float cos_angle = std::cos(angle);
            float sin_angle = std::sin(angle);
            V2f direction{ cos_facing * sin_angle - sin_facing * cos_angle, sin_facing * sin_angle + cos_facing * cos_angle };
            V2f hit_pos;
            V2f hit_normal;
            int hit_tile_id;
            cast_ray(camera_pos, direction, hit_pos, hit_normal, hit_tile_id);

            float dist = length(hit_pos - camera_pos) * sin_angle;
            float height = screen_dist / dist;
            int ceiling = (int)((screen_height() - height) * 0.5f);
            int floor = screen_height() - ceiling;

            if (ceiling < 0)
            {
                ceiling = 0;
            }

            for (int y = ceiling; y < screen_height() && y < floor; ++y)
            {
                float l = 0.5f;

                for (const Light& light : lights)
                {
                    V2f L = light.position - hit_pos;
                    float dist_sq = length_sq(L);
                    float intensity = light.intensity / dist_sq;

                    if (intensity > 0.01f)
                    {
                        L = L / std::sqrt(dist_sq);

                        V2f shadow_hit;
                        V2f shadow_normal;
                        int shadow_tile_id;
                        cast_ray(light.position, L, shadow_hit, shadow_normal, shadow_tile_id);
                        float shadow = 0.0f;

                        if (shadow_tile_id != hit_tile_id)
                        {
                            V2f S = light.position - shadow_hit;
                            if (length_sq(S) < dist_sq)
                            {
                                shadow = 1.0f;
                            }
                        }

                        float ndotl = dot(L, hit_normal);

                        if (ndotl > 0.0f)
                        {
                            l += intensity * ndotl * (1.0f - shadow);
                        }
                    }
                }

                uint8_t r = (uint8_t)std::floor(clamp((std::abs(hit_normal.x) * 128.0f * l) + 0.5f, 0.0f, 255.0f));
                uint8_t b = (uint8_t)std::floor(clamp((std::abs(hit_normal.y) * 128.0f * l) + 0.5f, 0.0f, 255.0f));
                uint8_t g = 0; //    (uint8_t)std::floor(clamp(l, 0.0f, 255.0f));
                set_pixel(i, y, gli::Pixel(r, g, b));
            }

            col_x++;
        }
    }

    void cast_ray(const V2f& origin, const V2f& direction, V2f& hit_pos, V2f& hit_normal, int& hit_tile_id) const
    {
        // Cast ray through the map and find the nearest intersection with a tile matching the filter
        float distance = FLT_MAX;
        V2f closest_hit_pos{};
        V2f closest_hit_normal{};
        int closest_tile_id = -1;

        if (direction.x > 0.0f)
        {
            float dydx = direction.y / direction.x;
            int tx = (int)(std::floor(origin.x + 1.0f));

            for (; tx < MapWidth; ++tx)
            {
                float y = origin.y + dydx * (tx - origin.x);
                int ty = (int)std::floor(y);

                if (ty < 0 || ty >= MapHeight)
                {
                    break;
                }

                if (solid(tx, ty))
                {
                    V2f hit{ (float)tx, (float)y };
                    float hit_d = length_sq(hit - origin);

                    if (hit_d < distance)
                    {
                        distance = hit_d;
                        closest_hit_pos = hit;
                        closest_hit_normal = V2f{ -1.0f, 0.0f };
                        closest_tile_id = ty * MapWidth + tx;
                    }

                    break;
                }
            }
        }
        else if (direction.x < 0.0f)
        {
            float dydx = direction.y / direction.x;
            int tx = (int)std::floor(origin.x - 1.0f);

            for (; tx >= 0; tx--)
            {
                float y = origin.y + dydx * (tx + 1.0f - origin.x);
                int ty = (int)std::floor(y);

                if (ty < 0 || ty >= MapHeight)
                {
                    break;
                }

                if (solid(tx, ty))
                {
                    V2f hit{ (float)(tx + 1), (float)y };
                    float hit_d = length_sq(hit - origin);

                    if (hit_d < distance)
                    {
                        distance = hit_d;
                        closest_hit_pos = hit;
                        closest_hit_normal = V2f{ 1.0f, 0.0f };
                        closest_tile_id = ty * MapWidth + tx;
                    }

                    break;
                }
            }
        }

        if (direction.y > 0.0f)
        {
            float dxdy = direction.x / direction.y;
            int ty = (int)std::floor(origin.y + 1.0f);

            for (; ty < MapHeight; ty++)
            {
                float x = origin.x + dxdy * (ty - origin.y);
                int tx = (int)std::floor(x);

                if (tx < 0 || tx >= MapWidth)
                {
                    break;
                }

                if (solid(tx, ty))
                {
                    V2f hit{ (float)x, (float)ty };
                    float hit_d = length_sq(hit - origin);

                    if (hit_d < distance)
                    {
                        distance = hit_d;
                        closest_hit_pos = hit;
                        closest_hit_normal = { 0.0f, -1.0f };
                        closest_tile_id = ty * MapWidth + tx;
                    }

                    break;
                }
            }
        }
        else if (direction.y < 0.0f)
        {
            float dxdy = direction.x / direction.y;
            int ty = (int)std::floor(origin.y - 1.0f);

            for (; ty >= 0; ty--)
            {
                float x = origin.x + dxdy * (ty + 1.0f - origin.y);
                int tx = (int)std::floor(x);

                if (tx < 0 || tx >= MapWidth)
                {
                    break;
                }

                if (solid(tx, ty))
                {
                    V2f hit{ (float)x, (float)(ty + 1) };
                    float hit_d = length_sq(hit - origin);

                    if (hit_d < distance)
                    {
                        distance = hit_d;
                        closest_hit_pos = hit;
                        closest_hit_normal = { 0.0f, 1.0f };
                        closest_tile_id = ty * MapWidth + tx;
                    }

                    break;
                }
            }
        }

        hit_pos = closest_hit_pos;
        hit_normal = closest_hit_normal;
        hit_tile_id = closest_tile_id;
    }

    enum BspState
    {
        Reset,
        Split,
        Idle
    };

    int bsp_state = Reset;

    struct BspLine
    {
        V2f a;
        V2f b;
        V2f n;

        static int side(const BspLine& split, const V2f& p)
        {
            // Return 1 if p is in postive half space, 0 if p lies on split, or -1 if p is in negative half space
            float d = dot(p, split.n) - dot(split.a, split.n);
            return (d > 0.0f) ? 1 : ((d < 0.0f) ? -1 : 0);
        }

        static int side(const BspLine& split, const BspLine& line)
        {
            // Return 1 if b is in a's postive half space, 0 if b intersects a or -1 if b is in a's negative half space
            int sa = side(split, line.a);
            int sb = side(split, line.b);

            if (sa == 0 && sb == 0)
            {
                return dot(split.n, line.n) < 0 ? -1 : 1;
            }

            if (sa >= 0 && sb >= 0)
            {
                return 1;
            }

            if (sa <= 0 && sb <= 0)
            {
                return -1;
            }

            return 0;
        }

        static void split(const BspLine& split, const BspLine& line, BspLine& front, BspLine& back)
        {
            float d0 = dot(line.a, split.n) - dot(split.a, split.n);
            float d1 = dot(line.b, split.n) - dot(split.a, split.n);

            if (d0 > d1)
            {
                float t = d0 / (d0 - d1);
                front.a = line.a;
                front.b = line.a + (line.b - line.a) * t;
                front.n = line.n;
                back.a = front.b;
                back.b = line.b;
                back.n = line.n;
            }
            else
            {
                float t = d1 / (d1 - d0);
                front.b = line.b;
                front.a = line.b + (line.a - line.b) * t;
                front.n = line.n;
                back.b = front.a;
                back.a = line.a;
                back.n = line.n;
            }
        }
    };


    struct BspSector
    {
        std::vector<size_t> lines;
    };

    enum BspNodeFlags
    {
        Leaf = 0x01
    };

    struct BspNode
    {
        BspLine split;
        uint32_t flags;
        size_t left;
        size_t right;
    };

    struct BspTree
    {
        std::vector<BspNode> nodes;
        std::vector<BspSector> sectors;
        std::vector<BspLine> lines;
    };

    struct BspTreeBuilder
    {
        struct BBox
        {
            V2f min{ FLT_MAX, FLT_MAX};
            V2f max{ -FLT_MAX, -FLT_MAX };
        };

        struct Sector
        {
            std::vector<BspLine> lines;
            BBox bbox;

            bool convex()
            {
                size_t num_lines = lines.size();

                for (size_t a = 0; a < num_lines; ++a)
                {
                    for (size_t b = a + 1; b < num_lines; ++b)
                    {
                        if (BspLine::side(lines[a], lines[b]) <= 0)
                        {
                            return false;
                        }
                    }
                }

                return true;
            }

            static void grow_bbox(BBox& box, const V2f& p)
            {
                box.min.x = std::min(p.x, box.min.x);
                box.min.y = std::min(p.y, box.min.y);
                box.max.x = std::max(p.x, box.max.x);
                box.max.y = std::max(p.y, box.max.y);
            }

            static void grow_bbox(BBox& box, const BspLine& l)
            {
                grow_bbox(box, l.a);
                grow_bbox(box, l.b);
            }

            void calc_bounds()
            {
                bbox = BBox();

                for (const BspLine& line : lines)
                {
                    grow_bbox(bbox, line);
                }
            }
        };

        struct Node
        {
            BspLine split;
            std::unique_ptr<Node> front;
            std::unique_ptr<Node> back;
            std::unique_ptr<Sector> sector;
        };

        std::deque<Node*> process_queue; // non-convex leaf nodes
        Node root;

        void init(const std::vector<BspLine>& lines)
        {
            root.sector = std::make_unique<Sector>();

            for (const BspLine& line : lines)
            {
                root.sector->lines.push_back(line);
            }

            root.sector->calc_bounds();

            if (!root.sector->convex())
            {
                process_queue.push_back(&root);
            }
        }

        struct SplitScoreData
        {
            size_t front{};  // lines in front
            size_t back{};  // lines behind
            size_t splits{}; // total splits
            BBox front_bound{};
            BBox back_bound{};
        };

        float calc_split_score(const SplitScoreData& data)
        {
            static const float balance_weight = 1.0f;
            static const float split_weight = 50.0f;
            static const float area_ratio_weight = 1.0f;

            float score = 0.0f;

            // balance
            size_t balance = (data.front > data.back) ? data.front - data.back : data.back - data.front;
            score -= balance * balance_weight;

            // splits are bad
            score -= data.splits * split_weight;

            // bbox ratio
            V2f front_extent = data.front_bound.max - data.front_bound.min;
            float front_area = front_extent.x * front_extent.y;
            V2f back_extent = data.back_bound.max - data.back_bound.min;
            float back_area = back_extent.x * back_extent.y;
            float area_ratio = (front_area > back_area) ? (back_area / front_area) : (front_area / back_area);
            score += area_ratio * area_ratio_weight;

            return score;
        }

        void split()
        {
            if (process_queue.empty())
            {
                return;
            }

            size_t best_split = (size_t)-1;   // index of the best split candidate
            float best_score = -FLT_MAX; // higher is better

            Node* node = process_queue.front();
            process_queue.pop_front();
            std::unique_ptr<Sector> sector = std::move(node->sector);
            size_t candidate_index = 0;
            
            for (const BspLine& candidate : sector->lines)
            {
                size_t test_index = 0;
                SplitScoreData score_data{};

                for (const BspLine& test : sector->lines)
                {
                    if (test_index == candidate_index)
                    {
                        score_data.front++;
                        Sector::grow_bbox(score_data.front_bound, test);
                    }
                    else
                    {
                        int side = BspLine::side(candidate, test);

                        if (side > 0)
                        {
                            score_data.front++;
                            Sector::grow_bbox(score_data.front_bound, test);
                        }
                        else if (side < 0)
                        {
                            score_data.back++;
                            Sector::grow_bbox(score_data.back_bound, test);
                        }
                        else
                        {
                            score_data.front++;
                            score_data.back++;
                            score_data.splits++;
                            BspLine front;
                            BspLine back;
                            BspLine::split(candidate, test, front, back);
                            Sector::grow_bbox(score_data.front_bound, front);
                            Sector::grow_bbox(score_data.back_bound, back);
                        }
                    }

                    test_index++;
                }

                if (score_data.back)
                {
                    float score = calc_split_score(score_data);
                    if (score > best_score)
                    {
                        best_split = candidate_index;
                        best_score = score;
                    }
                }

                candidate_index++;
            }

            if (best_split == (size_t)-1)
            {
                gliAssert(!"Bsp tree build failed, no split found.");
                return;
            }

            const BspLine& split = sector->lines[best_split];
            node->split = split;
            node->front = std::make_unique<Node>();
            node->back = std::make_unique<Node>();
            node->front->sector = std::make_unique<Sector>();
            node->back->sector = std::make_unique<Sector>();

            // Split sector to left & right
            size_t line_index = 0;

            for (const BspLine& line : sector->lines)
            {
                if (line_index == best_split)
                {
                    node->front->sector->lines.push_back(line);
                }
                else
                {
                    int side = BspLine::side(split, line);

                    if (side > 0)
                    {
                        node->front->sector->lines.push_back(line);
                    }
                    else if (side < 0)
                    {
                        node->back->sector->lines.push_back(line);
                    }
                    else
                    {
                        BspLine front;
                        BspLine back;
                        BspLine::split(split, line, front, back);
                        node->front->sector->lines.push_back(front);
                        node->back->sector->lines.push_back(back);
                    }
                }

                line_index++;
            }

            node->front->sector->calc_bounds();
            node->back->sector->calc_bounds();

            if (!node->front->sector->convex())
            {
                process_queue.push_back(node->front.get());
            }

            if (!node->back->sector->convex())
            {
                process_queue.push_back(node->back.get());
            }
        }

        void build()
        {
            while (!complete())
            {
                split();
            }
        }

        bool complete() { return process_queue.empty(); }
    };

    struct BspStateData
    {
        std::vector<BspLine> lines;
        std::unique_ptr<BspTreeBuilder> builder;
        BspTreeBuilder::Sector* current_sector;
    } bsp_data;

    void add_bsp_line(const V2f& a, const V2f& b)
    {
        for (BspLine& l : bsp_data.lines)
        {
            if (l.a.x == l.b.x && l.a.x == a.x && l.a.x == b.x)
            {
                if (l.a.y == b.y)
                {
                    l.a = a;
                    return;
                }
                else if (l.b.y == a.y)
                {
                    l.b = b;
                    return;
                }
            }
            else if (l.a.y == l.b.y && l.a.y == a.y && l.a.y == b.y)
            {
                if (l.a.x == b.x)
                {
                    l.a = a;
                    return;
                }
                else if (l.b.x == a.x)
                {
                    l.b = b;
                    return;
                }
            }
        }

        BspLine l{};
        l.a = a;
        l.b = b;

        V2f v = b - a;
        l.n.x = v.y;
        l.n.y = -v.x;
        bsp_data.lines.push_back(l);
    }

    void update_bsp(float delta)
    {
        auto sample_map = [&](int x, int y) -> char {
            char c = '#';
            if (x >= 0 && x < BspMapWidth && y >= 0 && y < BspMapHeight)
            {
                c = BspMap[x + y * BspMapWidth];
            }
            return c;
        };

        if (key_state(gli::Key_R).pressed)
        {
            // Reset state
            bsp_state = Reset;
        }

        if (bsp_state == BspState::Reset)
        {
            if (mouse_visible())
            {
                show_mouse(false);
            }

            bsp_data.lines.clear();
            bsp_data.builder = std::make_unique<BspTreeBuilder>();
            bsp_data.current_sector = nullptr;

            for (int y = 0; y < BspMapHeight; ++y)
            {
                for (int x = 0; x < BspMapWidth; ++x)
                {
                    if (sample_map(x, y) == '.')
                    {
                        if (sample_map(x - 1, y) == '#')
                        {
                            add_bsp_line(V2f{ (float)x, (float)y }, V2f{ (float)x, (float)y + 1.0f });
                        }
                        if (sample_map(x + 1, y) == '#')
                        {
                            add_bsp_line(V2f{ (float)x + 1.0f, (float)y + 1.0f }, V2f{ (float)x + 1.0f, (float)y });
                        }
                        if (sample_map(x, y - 1) == '#')
                        {
                            add_bsp_line(V2f{ (float)x + 1.0f, (float)y }, V2f{ (float)x, (float)y });
                        }
                        if (sample_map(x, y + 1) == '#')
                        {
                            add_bsp_line(V2f{ (float)x, (float)y + 1.0f }, V2f{ (float)x + 1.0f, (float)y + 1.0f });
                        }
                    }
                }
            }

            bsp_data.builder->init(bsp_data.lines);

            bsp_state = BspState::Split;
        }
        else if (bsp_state == BspState::Split)
        {
            if (key_state(gli::Key_Space).released)
            {
                bsp_data.builder->split();
            }

            if (bsp_data.builder->complete())
            {
                show_mouse(true);
                bsp_state = BspState::Idle;
            }
        }
        else if (bsp_state == BspState::Idle)
        {
            V2f mouse_world_pos = screen_to_world(V2i{mouse_state().x, mouse_state().y}, V2f{BspMapWidth, BspMapHeight} * 0.5f, bsp_tile_scale);
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

    int bsp_tile_scale = 64;

    int world_to_screen_x(float x, float ox, int tile_scale) { return (int)std::floor((x - ox) * tile_scale) + screen_width() / 2; }

    int world_to_screen_y(float y, float oy, int tile_scale) { return (int)std::floor((y - oy) * tile_scale) + screen_height() / 2; }

    V2i world_to_screen(const V2f& p, const V2f& o, int tile_scale)
    {
        return V2i{ world_to_screen_x(p.x, o.x, tile_scale), world_to_screen_y(p.y, o.y, tile_scale) };
    }

    V2f screen_to_world(const V2i& p, const V2f& o, int tile_scale)
    {
        V2i pn = p - V2i{ screen_width(), screen_height() } / 2;
        float oo_tile_scale = 1.0f / (float)tile_scale;
        return V2f{ pn.x * oo_tile_scale + o.x, pn.y * oo_tile_scale + o.y };
    }

    void draw_bsp_line(const BspLine& line, gli::Pixel color)
    {
        V2i from = world_to_screen(line.a, V2f{ BspMapWidth * 0.5f, BspMapHeight * 0.5f }, bsp_tile_scale);
        V2i to = world_to_screen(line.b, V2f{ BspMapWidth * 0.5f, BspMapHeight * 0.5f }, bsp_tile_scale);
        V2i mid = (from + to) / 2;
        V2i ntick = mid + V2i{ (int)std::floor(line.n.x * 10.0f), (int)std::floor(line.n.y * 10.0f) };
        draw_line(from.x, from.y, to.x, to.y, color);
        draw_line(mid.x, mid.y, ntick.x, ntick.y, color);
        draw_line(from.x - 2, from.y - 2, from.x + 3, from.y + 3, color);
        draw_line(from.x - 2, from.y + 2, from.x + 3, from.y - 3, color);
    }

    void draw_bsp_split(const BspLine& split, const BspTreeBuilder::BBox& bound)
    {
        BspLine line = split;

        if (std::abs(split.n.x) > std::abs(split.n.y))
        {
            line.a.y = bound.min.y;
            line.b.y = bound.max.y;
        }
        else
        {
            line.a.x = bound.min.x;
            line.b.x = bound.max.x;
        }

        draw_bsp_line(line, gli::Pixel(128, 128, 128));
    }

    void render_bsp_sector(BspTreeBuilder::Sector* sector, bool highlight)
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

    void render_bsp_node(BspTreeBuilder::Node* node, const BspTreeBuilder::BBox& bbox)
    {
        if (node->sector)
        {
            render_bsp_sector(node->sector.get(), false);
        }
        else
        {
            BspTreeBuilder::BBox bbox_front = bbox;
            BspTreeBuilder::BBox bbox_back = bbox;
            float split_x = node->split.a.x;
            float split_y = node->split.a.y;

            if (node->split.n.x > 0)
            {
                bbox_front.min.x = split_x;
                bbox_back.max.x = split_x;
            }
            else if (node->split.n.x < 0)
            {
                bbox_front.max.x = split_x;
                bbox_back.min.x = split_x;
            }
            else if (node->split.n.y > 0)
            {
                bbox_front.min.y = split_y;
                bbox_back.max.y = split_y;
            }
            else if (node->split.n.y < 0)
            {
                bbox_front.max.y = split_y;
                bbox_back.min.y = split_y;
            }
            else
            {
                gliAssert(!"Illegal split normal");
            }

            render_bsp_node(node->front.get(), bbox_front);
            render_bsp_node(node->back.get(), bbox_back);

            set_blend_mode(gli::BlendMode::One, gli::BlendMode::One, 0);
            set_blend_op(gli::BlendOp::Add);
            draw_bsp_split(node->split, bbox);
            set_blend_op(gli::BlendOp::None);
        }
    }

    void render_bsp(float delta)
    {
        clear_screen(gli::Pixel(0, 0, 32));

        for (int y = 0; y < BspMapHeight; ++y)
        {
            int ty = world_to_screen_y((float)y, BspMapHeight * 0.5f, bsp_tile_scale);

            for (int x = 0; x < BspMapWidth; ++x)
            {
                int tx = world_to_screen_x((float)x, BspMapWidth * 0.5f, bsp_tile_scale);

                if (BspMap[x + y * BspMapWidth] == '#')
                {
                    fill_rect(tx, ty, bsp_tile_scale, bsp_tile_scale, 0, gli::Pixel(16, 16, 48), 0);
                }
            }
        }

        BspTreeBuilder::BBox bbox;
        bbox.min = V2f{};
        bbox.max = V2f{ (float)BspMapWidth, (float)BspMapHeight };
        render_bsp_node(&bsp_data.builder->root, bbox);

        if (bsp_data.current_sector)
        {
            render_bsp_sector(bsp_data.current_sector, true);
        }
    }
};

Fist app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Fist", 1280, 720, 1))
    {
        app.run();
    }

    return 0;
}
