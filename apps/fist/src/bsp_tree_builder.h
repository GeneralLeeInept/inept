#pragma once

#include "types.h"

#include <deque>
#include <memory>
#include <vector>

namespace fist
{

struct BspLine
{
    V2f a;
    V2f b;
    V2f n;

    static int side(const BspLine& split, const V2f& p);
    static int side(const BspLine& split, const BspLine& line);
    static bool split(const BspLine& split, const BspLine& line, BspLine& front, BspLine& back);
};

struct BspTreeBuilder
{
    struct BBox
    {
        V2f min{ FLT_MAX, FLT_MAX };
        V2f max{ -FLT_MAX, -FLT_MAX };
    };

    struct Sector
    {
        std::vector<BspLine> lines;
        BBox bbox;

        bool convex();
        static void grow_bbox(BBox& box, const V2f& p);
        static void grow_bbox(BBox& box, const BspLine& l);
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
    void split();
    void build();
    bool complete();

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
        BBox front_bound{};
        BBox back_bound{};
        bool ortho{};
    };

    SplitScoreWeights split_score_weights{};

    float calc_split_score(const SplitScoreData& data);
};

}
