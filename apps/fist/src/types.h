#pragma once

#include <cfloat>
#include <cmath>
#include <cstdint>

namespace fist
{

struct V2f
{
    float x;
    float y;
};


struct Rectf
{
    V2f origin;
    V2f extents;
};


struct V2i
{
    int x;
    int y;
};


struct Recti
{
    V2i origin;
    V2i extents;
};


struct BoundingBox
{
    V2f mins{ FLT_MAX, FLT_MAX };
    V2f maxs{ -FLT_MAX, -FLT_MAX };

    float area() const;

    // Expand to include p
    void grow(const V2f& p);

    // Get the union of a & b
    static BoundingBox merge(const BoundingBox& a, const BoundingBox& b);

    // Get the intersection of a & b
    static BoundingBox intersection(const BoundingBox& a, const BoundingBox& b);
};


struct Mat2
{
    V2f x{ 1.0f, 0.0f };
    V2f y{ 0.0f, 1.0f };
};


struct Transform2D
{
    Mat2 m{};
    V2f p{};
};


struct ThingPos
{
    V2f p{};   // X/Y position
    float f{}; // Facing angle relative to +X axis
    float h{}; // Height (Z position)
};


struct Player
{
    ThingPos pos{};
};


static const float PI = 3.14159274101257324f;

V2f operator+(const V2f& a, const V2f& b);
V2f operator-(const V2f& a, const V2f& b);
V2f operator*(const V2f& v, float s);
V2f operator/(const V2f& v, float s);
V2f operator*(const V2f& a, const V2f& b);
V2f operator/(const V2f& a, const V2f& b);
V2f operator-(const V2f& a);
bool operator==(const V2f& a, const V2f& b);
float length_sq(const V2f& v);
float length(const V2f& v);
V2f normalize(const V2f& v);
float dot(const V2f& a, const V2f& b);
float cross(const V2f& a, const V2f& b);
V2f operator*(const Mat2& m, const V2f& v);
V2f operator*(const Transform2D& t, const V2f& p);
Mat2 transpose(const Mat2& m);
V2i operator+(const V2i& a, const V2i& b);
V2i operator-(const V2i& a, const V2i& b);
V2i operator*(const V2i& v, int s);
V2i operator/(const V2i& v, int s);

Transform2D inverse(const Transform2D& t);
Transform2D from_camera(const V2f& p, float facing);

inline constexpr float clamp(float t, float min, float max)
{
    return t < min ? min : (t > max ? max : t);
}

inline constexpr float deg_to_rad(float deg)
{
    const float conv = PI / 180.0f;
    return deg * conv;
}

inline constexpr float rad_to_deg(float rad)
{
    const float conv = 180.0f / PI;
    return rad * conv;
}

} // namespace fist
