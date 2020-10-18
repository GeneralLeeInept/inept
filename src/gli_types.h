#pragma once

#include <cstdint>
#include <string>

namespace gli
{
template <typename T>
T clamp(const T& v, const T& min, const T& max)
{
    return (v < min) ? min : ((v > max) ? max : v);
}

template <typename T>
T lerp(const T& from, const T& to, const T& t)
{
    return from + (to - from) * t;
}
}
