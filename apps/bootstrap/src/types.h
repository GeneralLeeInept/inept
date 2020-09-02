#pragma once

#include <cmath>
#include <cstdint>

namespace Bootstrap
{

struct V2f
{
    float x;
    float y;
};


inline V2f operator+(const V2f& a, const V2f& b)
{
    return { a.x + b.x, a.y + b.y };
}


inline V2f operator-(const V2f& a, const V2f& b)
{
    return { a.x - b.x, a.y - b.y };
}


inline V2f operator*(const V2f& v, float s)
{
    return { v.x * s, v.y * s };
}


inline float length_sq(const V2f& v)
{
    return (v.x * v.x) + (v.y * v.y);
}


inline float length(const V2f& v)
{
    return std::sqrt(length_sq(v));
}


inline V2f normalize(const V2f& v)
{
    return v * (1.0f / length(v));
}


}