#pragma once

#include <cmath>
#include <cstdint>

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


inline V2f operator/(const V2f& v, float s)
{
    return { v.x / s, v.y / s };
}


inline V2f operator*(const V2f& a, const V2f& b)
{
    return { a.x * b.x, a.y * b.y };
}


inline V2f operator/(const V2f& a, const V2f& b)
{
    return { a.x / b.x, a.y / b.y };
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


inline float dot(const V2f& a, const V2f& b)
{
    return (a.x * b.x) + (a.y * b.y);
}


inline V2i operator+(const V2i& a, const V2i& b)
{
    return { a.x + b.x, a.y + b.y };
}


inline V2i operator-(const V2i& a, const V2i& b)
{
    return { a.x - b.x, a.y - b.y };
}


inline V2i operator*(const V2i& v, int s)
{
    return { v.x * s, v.y * s };
}


inline V2i operator/(const V2i& v, int s)
{
    return { v.x / s, v.y / s };
}

inline float clamp(float t, float min, float max)
{
    return t < min ? min : (t > max ? max : t);
}
