#include "types.h"

namespace fist
{

float BoundingBox::area() const
{
    float result = 0.0f;

    if (maxs.x > mins.x && maxs.y > mins.y)
    {
        V2f extents = maxs - mins;
        result = extents.x * extents.y;
    }

    return result;
}

void BoundingBox::grow(const V2f& p)
{
    mins.x = p.x < mins.x ? p.x : mins.x;
    mins.y = p.y < mins.y ? p.y : mins.y;
    maxs.x = p.x > maxs.x ? p.x : maxs.x;
    maxs.y = p.y > maxs.y ? p.y : maxs.y;
}

BoundingBox BoundingBox::merge(const BoundingBox& a, const BoundingBox& b)
{
    BoundingBox result;
    result.mins.x = a.mins.x < b.mins.x ? a.mins.x : b.mins.x;
    result.mins.y = a.mins.y < b.mins.y ? a.mins.y : b.mins.y;
    result.maxs.x = a.maxs.x > b.maxs.x ? a.maxs.x : b.maxs.x;
    result.maxs.y = a.maxs.y > b.maxs.y ? a.maxs.y : b.maxs.y;
    return result;
}

// Get the intersection of a & b
BoundingBox BoundingBox::intersection(const BoundingBox& a, const BoundingBox& b)
{
    BoundingBox result;
    result.mins.x = a.mins.x < b.mins.x ? a.mins.x : b.mins.x;
    result.mins.y = a.mins.y < b.mins.y ? a.mins.y : b.mins.y;
    result.maxs.x = a.maxs.x > b.maxs.x ? a.maxs.x : b.maxs.x;
    result.maxs.y = a.maxs.y > b.maxs.y ? a.maxs.y : b.maxs.y;
    return result;
}

V2f operator+(const V2f& a, const V2f& b)
{
    return { a.x + b.x, a.y + b.y };
}


V2f operator-(const V2f& a, const V2f& b)
{
    return { a.x - b.x, a.y - b.y };
}


V2f operator*(const V2f& v, float s)
{
    return { v.x * s, v.y * s };
}


V2f operator/(const V2f& v, float s)
{
    return { v.x / s, v.y / s };
}


V2f operator*(const V2f& a, const V2f& b)
{
    return { a.x * b.x, a.y * b.y };
}


V2f operator/(const V2f& a, const V2f& b)
{
    return { a.x / b.x, a.y / b.y };
}

V2f operator-(const V2f& a)
{
    return { -a.x, -a.y };
}

bool operator==(const V2f& a, const V2f& b)
{
    return a.x == b.x && a.y == b.y;
}

float length_sq(const V2f& v)
{
    return (v.x * v.x) + (v.y * v.y);
}


float length(const V2f& v)
{
    return std::sqrt(length_sq(v));
}


V2f normalize(const V2f& v)
{
    return v * (1.0f / length(v));
}


float dot(const V2f& a, const V2f& b)
{
    return (a.x * b.x) + (a.y * b.y);
}

float cross(const V2f& a, const V2f& b)
{
    return (a.x * b.y - a.y * b.x);
}

V2i operator+(const V2i& a, const V2i& b)
{
    return { a.x + b.x, a.y + b.y };
}


V2i operator-(const V2i& a, const V2i& b)
{
    return { a.x - b.x, a.y - b.y };
}


V2i operator*(const V2i& v, int s)
{
    return { v.x * s, v.y * s };
}


V2i operator/(const V2i& v, int s)
{
    return { v.x / s, v.y / s };
}

V2f operator*(const Mat2& m, const V2f& v)
{
    V2f result;
    result.x = dot(V2f{ m.x.x, m.y.x }, v);
    result.y = dot(V2f{ m.x.y, m.y.y }, v);
    return result;
}

V2f operator*(const Transform2D& t, const V2f& p)
{
    return t.m * p + t.p;
}

Mat2 transpose(const Mat2& m)
{
    Mat2 t;
    t.x.x = m.x.x;
    t.x.y = m.y.x;
    t.y.x = m.x.y;
    t.y.y = m.y.y;
    return t;
}

Transform2D inverse(const Transform2D& t)
{
    Mat2 m = transpose(t.m);
    V2f p = -(m * t.p);
    return { m, p };
}

Transform2D from_camera(const V2f& p, float facing)
{
    Transform2D t;
    float cos_facing = std::cos(facing);
    float sin_facing = std::sin(facing);

    // Forward
    t.m.y.x = cos_facing;
    t.m.y.y = sin_facing;

    // Right
    t.m.x.x = sin_facing;
    t.m.x.y = -cos_facing;

    t.p = p;
    return t;
}

} // namespace fist
