#include "bsp_tree_builder.h"

#include "types.h"
#include "gli.h"

#include <algorithm>

namespace fist
{

int BspLine::side(const BspLine& split, const V2f& p)
{
    // Return 1 if p is in postive half space, 0 if p lies on split, or -1 if p is in negative half space
    const float epsilon = 1.0e-3f;
    float d = dot(p, split.n) - dot(split.a, split.n);
    return (std::abs(d) <= epsilon) ? 0 : ((d > 0.0f) ? 1 : -1);
}

int BspLine::side(const BspLine& split, const BspLine& line)
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

bool BspLine::split(const BspLine& split, const BspLine& line, BspLine& front, BspLine& back)
{
    float d0 = dot(line.a, split.n) - dot(split.a, split.n);
    float d1 = dot(line.b, split.n) - dot(split.a, split.n);

    if ((d0 >= 0.0f && d1 >= 0.0f) || (d0 <= 0.0f && d1 <= 0.0f))
    {
        return false;
    }

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

void BspTreeBuilder::Sector::grow_bbox(BBox& box, const V2f& p)
{
    box.min.x = std::min(p.x, box.min.x);
    box.min.y = std::min(p.y, box.min.y);
    box.max.x = std::max(p.x, box.max.x);
    box.max.y = std::max(p.y, box.max.y);
}

void BspTreeBuilder::Sector::grow_bbox(BBox& box, const BspLine& l)
{
    grow_bbox(box, l.a);
    grow_bbox(box, l.b);
}

void BspTreeBuilder::Sector::calc_bounds()
{
    bbox = BBox();

    for (const BspLine& line : lines)
    {
        grow_bbox(bbox, line);
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

float BspTreeBuilder::calc_split_score(const SplitScoreData& data)
{
    float score = 0.0f;

    // balance
    size_t balance = (data.front > data.back) ? data.front - data.back : data.back - data.front;
    score -= balance * split_score_weights.balance_weight;

    // splits are bad
    score -= data.splits * split_score_weights.split_weight;

    // bbox ratio
    V2f front_extent = data.front_bound.max - data.front_bound.min;
    float front_area = front_extent.x * front_extent.y;
    V2f back_extent = data.back_bound.max - data.back_bound.min;
    float back_area = back_extent.x * back_extent.y;
    float area_ratio = 1.0f;
    if (front_area != back_area)
    {
        area_ratio = (front_area > back_area) ? (back_area / front_area) : (front_area / back_area);
    }
    score += area_ratio * split_score_weights.area_ratio_weight;

    // bonus...
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
            score_data.ortho = std::abs(candidate.n.x) < 1.0e-6f || std::abs(candidate.n.y) < 1.0e-6f;
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

} // namespace fist
