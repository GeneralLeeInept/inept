#include "bsp_tree_builder.h"

#include "bsp.h"
#include "types.h"
#include "wad_loader.h"

#include <gli.h>
#include <algorithm>

namespace fist
{

int BspLine::side(const BspLine& split, const V2f& p)
{
    // Return 1 if p is in postive half space, 0 if p lies on split, or -1 if p is in negative half space
    const float epsilon = 1.0e-3f;
    float d = dot(p, split.normal) - dot(split.from, split.normal);
    return (std::abs(d) <= epsilon) ? 0 : ((d > 0.0f) ? 1 : -1);
}

int BspLine::side(const BspLine& split, const BspLine& line)
{
    // Return 1 if b is in a's postive half space, 0 if b intersects a or -1 if b is in a's negative half space
    int sa = side(split, line.from);
    int sb = side(split, line.to);

    if (sa == 0 && sb == 0)
    {
        return dot(split.normal, line.normal) < 0 ? -1 : 1;
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

bool BspLine::split(const BspLine& split, const BspLine& line, BspLine& front, BspLine& back)
{
    float d0 = dot(line.from, split.normal) - dot(split.from, split.normal);
    float d1 = dot(line.to, split.normal) - dot(split.from, split.normal);

    if ((d0 >= 0.0f && d1 >= 0.0f) || (d0 <= 0.0f && d1 <= 0.0f))
    {
        return false;
    }

    if (d0 > d1)
    {
        float t = d0 / (d0 - d1);
        front.from = line.from;
        front.to = line.from + (line.to - line.from) * t;
        front.normal = line.normal;
        back.from = front.to;
        back.to = line.to;
        back.normal = line.normal;
    }
    else
    {
        float t = d1 / (d1 - d0);
        front.to = line.to;
        front.from = line.to + (line.from - line.to) * t;
        front.normal = line.normal;
        back.to = front.from;
        back.from = line.from;
        back.normal = line.normal;
    }

    front.linedef = line.linedef;
    back.linedef = line.linedef;

    return true;
}

bool BspTreeBuilder::Sector::convex()
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

void BspTreeBuilder::Sector::calc_bounds()
{
    bounds = BoundingBox();

    for (const BspLine& line : lines)
    {
        bounds.grow(line.from);
        bounds.grow(line.to);
    }
}

void BspTreeBuilder::init(const std::vector<BspLine>& lines)
{
    root.parent = nullptr;
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

V2f quantize(const V2f& pos)
{
    // Doom is ~32 units / m
    const float pos_scale = 1.0f / 32.0f;

    // quantize to 1/1024th meter
    int ix = (int)std::floor(std::abs(pos.x * pos_scale) * 1024.0f);
    int iy = (int)std::floor(std::abs(pos.y * pos_scale) * 1024.0f);

    float qx = (pos.x < 0.0f) ? (ix / -1024.0f) : (ix / 1024.0f);
    float qy = (pos.y < 0.0f) ? (iy / -1024.0f) : (iy / 1024.0f);

    return V2f{ qx, qy };
}

V2f calculate_normal(const V2f& from, const V2f& to)
{
    V2f v = normalize(to - from);
    return { v.y, -v.x };
}

void BspTreeBuilder::init(const Wad::Map& wad_map)
{
    std::vector<V2f> vertices;
    vertices.reserve(wad_map.vertices.size());

    for (const Wad::Vertex& v : wad_map.vertices)
    {
        vertices.push_back(quantize(V2f{ (float)v.x, (float)v.y }));
    }

    std::vector<BspLine> lines;
    for (size_t li = 0; li < wad_map.linedefs.size(); ++li)
    {
        const Wad::LineDef& ld = wad_map.linedefs[li];

        BspLine bsp_line{};
        bsp_line.from = vertices[ld.from];
        bsp_line.to = vertices[ld.to];
        bsp_line.normal = calculate_normal(bsp_line.from, bsp_line.to);
        bsp_line.linedef = li;
        bsp_line.front = true;
        lines.push_back(bsp_line);

        if (ld.sidedefs[1] != 0xffff)
        {
            BspLine bsp_line{};
            bsp_line.from = vertices[ld.to];
            bsp_line.to = vertices[ld.from];
            bsp_line.normal = calculate_normal(bsp_line.from, bsp_line.to);
            bsp_line.linedef = li;
            bsp_line.front = false;
            lines.push_back(bsp_line);
        }
    }

    init(lines);
}

float BspTreeBuilder::calc_split_score(const SplitScoreData& data)
{
    float score = 0.0f;

    // balance
    size_t balance = (data.front > data.back) ? data.front - data.back : data.back - data.front;
    score -= balance * split_score_weights.balance_weight;

    // splits are bad
    score -= data.splits * split_score_weights.split_weight;

    // bbox ratio
    float front_area = data.front_bound.area();
    float back_area = data.back_bound.area();
    float area_ratio = 1.0f;

    if (front_area != back_area)
    {
        area_ratio = (front_area > back_area) ? (back_area / front_area) : (front_area / back_area);
    }

    score += area_ratio * split_score_weights.area_ratio_weight;

    // orthogonal bonus
    if (data.ortho)
    {
        score += split_score_weights.orthogonal_bonus;
    }

    return score;
}

void BspTreeBuilder::split()
{
    if (process_queue.empty())
    {
        return;
    }

    size_t best_split = (size_t)-1; // index of the best split candidate
    float best_score = -FLT_MAX;    // higher is better

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
                score_data.front_bound.grow(test.from);
                score_data.front_bound.grow(test.to);
            }
            else
            {
                int side = BspLine::side(candidate, test);

                if (side > 0)
                {
                    score_data.front++;
                    score_data.front_bound.grow(test.from);
                    score_data.front_bound.grow(test.to);
                }
                else if (side < 0)
                {
                    score_data.back++;
                    score_data.back_bound.grow(test.from);
                    score_data.back_bound.grow(test.to);
                }
                else
                {
                    score_data.front++;
                    score_data.back++;
                    score_data.splits++;
                    BspLine front;
                    BspLine back;
                    BspLine::split(candidate, test, front, back);
                    score_data.front_bound.grow(front.from);
                    score_data.front_bound.grow(front.to);
                    score_data.front_bound.grow(back.from);
                    score_data.front_bound.grow(back.to);
                }
            }

            test_index++;
        }

        if (score_data.back)
        {
            score_data.ortho = std::abs(candidate.normal.x) < 1.0e-6f || std::abs(candidate.normal.y) < 1.0e-6f;
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
    node->front->parent = node;
    node->back = std::make_unique<Node>();
    node->back->parent = node;
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

    if (!node->back->sector->convex())
    {
        process_queue.push_front(node->back.get());
    }

    if (!node->front->sector->convex())
    {
        process_queue.push_front(node->front.get());
    }
}

void BspTreeBuilder::build()
{
    while (!complete())
    {
        split();
    }
}

bool BspTreeBuilder::complete()
{
    return process_queue.empty();
}

uint32_t add_vertex(const V2f& v, std::vector<V2f>& vertices)
{
    size_t result = (size_t)-1;

    for (auto it = vertices.begin(); it != vertices.end(); ++it)
    {
        if (*it == v)
        {
            result = it - vertices.begin();
            break;
        }
    }

    if (result == (size_t)-1)
    {
        result = vertices.size();
        vertices.push_back(v);
    }

    gliAssert(result <= UINT32_MAX);
    return (uint32_t)result;
}

uint32_t add_vertex(const Wad::Vertex& vertex, std::vector<V2f>& vertices)
{
    V2f v = quantize(V2f{ (float)vertex.x, (float)vertex.y });
    return add_vertex(v, vertices);
}

void count_the_things(const BspTreeBuilder::Node* builder_node, fist::Map& map)
{
    if (builder_node->sector)
    {
        map.num_subsectors++;
        map.num_linesegs += (uint32_t)builder_node->sector->lines.size();
    }
    else
    {
        map.num_nodes++;
        count_the_things(builder_node->front.get(), map);
        count_the_things(builder_node->back.get(), map);
    }
}

void count_the_things(const BspTreeBuilder& builder, fist::Map& map)
{
    map.num_linesegs = 0;
    map.num_subsectors = 0;
    map.num_nodes = 0;
    count_the_things(&builder.root, map);
}

uint32_t add_sub_sector(const BspTreeBuilder::Sector* builder_sector, fist::Map& map, std::vector<V2f>& vertices)
{
    uint32_t index = map.num_subsectors++;
    SubSector* subsector = &map.subsectors[index];
    subsector->first_seg = map.num_linesegs;

    for (const BspLine& line : builder_sector->lines)
    {
        LineSeg* lineseg = &map.linesegs[map.num_linesegs++];
        lineseg->from = add_vertex(line.from, vertices);
        lineseg->to = add_vertex(line.to, vertices);
        lineseg->side = line.front ? 0 : 1;
        lineseg->line_def = (uint32_t)line.linedef;
    }

    subsector->num_segs = map.num_linesegs - subsector->first_seg;

    return index;
}

uint32_t add_node(const BspTreeBuilder::Node* builder_node, fist::Map& map, std::vector<V2f>& vertices)
{
    uint32_t index = 0xffffffff;

    if (builder_node &&builder_node->sector)
    {
        index = SubSectorBit | add_sub_sector(builder_node->sector.get(), map, vertices);
    }
    else if (builder_node)
    {
        index = map.num_nodes++;
        fist::Node* node = &map.nodes[index];
        node->split_normal = builder_node->split.normal;
        node->split_distance = dot(builder_node->split.normal, builder_node->split.from);
        node->right_child = add_node(builder_node->front.get(), map, vertices);
        node->left_child = add_node(builder_node->back.get(), map, vertices);
    }

    return index;
}

void gather_the_things(const BspTreeBuilder& builder, fist::Map& map, std::vector<V2f>& vertices)
{
    map.num_linesegs = 0;
    map.num_subsectors = 0;
    map.num_nodes = 0;
    add_node(&builder.root, map, vertices);
}

// FIXME: This won't work if the map consists of a single convex sector because the builder won't generate a root node in that case.
void BspTreeBuilder::cook(const Wad::Map& wad_map, fist::Map& map)
{
    BspTreeBuilder builder;
    builder.init(wad_map);
    builder.build();

    std::vector<V2f> vertices{};

    map.num_linedefs = (uint32_t)wad_map.linedefs.size();
    map.linedefs = new LineDef[map.num_linedefs];
    LineDef* linedef =  map.linedefs;

    for (const Wad::LineDef& src : wad_map.linedefs)
    {
        linedef->from = add_vertex(wad_map.vertices[src.from], vertices);
        linedef->to = add_vertex(wad_map.vertices[src.to], vertices);
        linedef->sidedefs[0] = src.sidedefs[0];
        linedef->sidedefs[1] = (src.sidedefs[1] == 0xffff) ? 0xffffffff : src.sidedefs[1];
        linedef++;
    }

    map.num_sidedefs = (uint32_t)wad_map.sidedefs.size();
    map.sidedefs = new SideDef[map.num_sidedefs];
    SideDef* sidedef = map.sidedefs;

    for (const Wad::SideDef& src : wad_map.sidedefs)
    {
        // TODO: Textures
        sidedef->sector = src.sector;
        sidedef++;
    }

    map.num_sectors = (uint32_t)wad_map.sectors.size();
    map.sectors = new fist::Sector[map.num_sectors];
    fist::Sector* sector = map.sectors;

    for (const Wad::Sector& src : wad_map.sectors)
    {
        // TODO: Textures
        sector->ceiling_height = src.ceiling / 32.0f;
        sector->floor_height = src.floor / 32.0f;
        sector->light_level = src.light / 255.0f;
        sector++;
    }

    count_the_things(builder, map);

    map.linesegs = new LineSeg[map.num_linesegs];
    map.subsectors = new SubSector[map.num_subsectors];
    map.nodes = new fist::Node[map.num_nodes];

    gather_the_things(builder, map, vertices);

    map.num_vertices = (uint32_t)vertices.size();
    map.vertices = new V2f[map.num_vertices];
    V2f* vertex = map.vertices;

    for (V2f& src : vertices)
    {
        *vertex++ = src;
    }
}

} // namespace fist
