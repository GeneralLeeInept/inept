#pragma once

#include "types.h"

#include <deque>
#include <memory>
#include <vector>

namespace fist
{

namespace Wad
{
struct Map;
}

struct Map;

struct BspLine
{
    V2f from;
    V2f to;
    V2f normal;
    size_t linedef;
    bool front;

    static int side(const BspLine& split, const V2f& p);
    static int side(const BspLine& split, const BspLine& line);
    static bool split(const BspLine& split, const BspLine& line, BspLine& front, BspLine& back);
};

struct BspTreeBuilder
{
    struct Sector
    {
        std::vector<BspLine> lines;
        BoundingBox bounds;

        bool convex();
        void calc_bounds();
    };

    struct Node
    {
        BspLine split;
        Node* parent;
        std::unique_ptr<Node> front;
        std::unique_ptr<Node> back;
        std::unique_ptr<Sector> sector;
    };

    std::deque<Node*> process_queue; // non-convex leaf nodes
    Node root;

    void init(const std::vector<BspLine>& lines);
    void init(const Wad::Map& wad_map);
    void split();
    void build();
    bool complete();

    static void cook(const Wad::Map& wad_map, fist::Map& map);

    struct SplitScoreWeights
    {
        float balance_weight{2.0f};
        float split_weight{1.0f};
        float area_ratio_weight{1.0f};
        float orthogonal_bonus{100.0f};
    };

    struct SplitScoreData
    {
        size_t front{};  // lines in front
        size_t back{};   // lines behind
        size_t splits{}; // total splits
        BoundingBox front_bound{};
        BoundingBox back_bound{};
        bool ortho{};
    };

    SplitScoreWeights split_score_weights{};

    float calc_split_score(const SplitScoreData& data);
};

}
