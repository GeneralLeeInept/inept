#include "collision.h"

namespace Bootstrap
{

bool contains(const Rect& rect, const V2f& p)
{
    if (p.x < rect.origin.x || p.y < rect.origin.y)
    {
        return false;
    }

    V2f max = rect.origin + rect.extents;

    if (p.x > max.x || p.y > max.y)
    {
        return false;
    }

    return true;
}


bool swept_circle_vs_circle(const V2f& a, const V2f& b, float r1, const V2f& p, float r2)
{
    // Normalized distance from a along the line through a->b of the closest point to p
    V2f ab = b - a;
    V2f ap = p - a;
    float t = dot(ap, ab) / dot(ab, ab);

    // Find closest point on the segment to p
    V2f c;

    if (t <= 0.0f)
    {
        // a is the closest point
        c = a;
    }
    else if (t >= 1.0f)
    {
        // b is the closest point
        c = b;
    }
    else
    {
        c = a + ab * t;
    }

    // Distance between closest point and p
    float d = length_sq(p - c);

    // Collision if d < r1 + r2
    return d <= (r1 + r2) * (r1 + r2);
}

}
