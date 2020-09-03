#pragma once

#include "types.h"

namespace Bootstrap
{

bool contains(const Rectf& rect, const V2f& p);
bool contains(const Recti& rect, const V2i& p);
bool swept_circle_vs_circle(const V2f& a, const V2f& b, float r1, const V2f& p, float r2);

}