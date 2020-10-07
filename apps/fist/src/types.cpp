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

} // namespace fist
