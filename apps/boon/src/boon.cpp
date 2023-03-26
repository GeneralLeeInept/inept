#include <gli.h>

#include <cmath>

namespace boon
{
struct Vertex
{
    float x;
    float y;
};

struct Wall
{
    Vertex from;
    Vertex to;
};

static constexpr float PI = 3.14159265379f;
static constexpr float PI_2 = PI * 0.5f;
static constexpr float TAU = 2.0f * PI;

static constexpr float DegToRad(float deg)
{
    return (deg * PI) / 180.0f;
}

static constexpr Vertex operator-(const Vertex& a, const Vertex& b)
{
    return { (b.x - a.x), (b.y - a.y) };
}

static Wall s_walls[] = {
    { { -5.0f, 5.0f }, { 5.0f, 5.0f } },
    { { 5.0f, 5.0f }, { 5.0f, -5.0f } },
    { { 5.0f, -5.0f }, { -5.0f, -5.0f } },
    { { -5.0f, -5.0f }, { -5.0f, 5.0f } }
};

static constexpr int s_wallCount = sizeof(s_walls) / sizeof(s_walls[0]);

class boon : public gli::App
{
public:
    bool on_create() override { return true; }

    void on_destroy() override {}

    bool on_update(float delta) override
    {
        clear_screen(0);

        m_t += delta * 1.f;;

        if (m_t >= TAU)
        {
            m_t -= TAU;
        }

        m_heading += delta;

        if (m_heading >= TAU)
        {
            m_heading -= TAU;
        }

        m_position.y = 3.0f * std::sin(m_t);
        m_position.x = 3.0f * std::cos(m_t);

        m_aspectRatio = (float)screen_width() / (float)screen_height();
        m_screenDistance = 1.0f / std::tan(m_fovY * 0.5f);

        for (int i = 0; i < s_wallCount; ++i)
        {
            Wall w;

            if (ClipWall(w, s_walls[i]))
            {
                DrawWall(w, i);
            }
        }

        return true;
    }

private:
    Vertex ToViewSpace(const Vertex& v)
    {
        Vertex vp = v - m_position;
        float c = std::cos(m_heading);
        float s = std::sin(m_heading);
        return Vertex { vp.x * c - vp.y * s, vp.x * s + vp.y * c };
    }

    bool ClipWall(Wall& clipped, const Wall& w)
    {
        Vertex from = ToViewSpace(w.from);
        Vertex to = ToViewSpace(w.to);

        if (from.y < m_screenDistance && to.y < m_screenDistance)
        {
            return false;
        }
        else if (from.y < m_screenDistance)
        {
            float dx = to.x - from.x;
            float dy = to.y - from.y;
            float dyp = to.y - m_screenDistance;
            float dxp = dyp * dx / dy;
            from.x = to.x - dxp;
            from.y = m_screenDistance;
        }
        else if (to.y < m_screenDistance)
        {
            float dx = from.x - to.x;
            float dy = from.y - to.y;
            float dyp = from.y - m_screenDistance;
            float dxp = dyp * dx / dy;
            to.x = from.x - dxp;
            to.y = m_screenDistance;
        }

        clipped = { from, to };
        return true;
    }

    void DrawWall(const Wall& w, int color)
    {
        float wallHeight = 2.f;
        float vx1 = m_screenDistance * w.from.x / w.from.y;
        float vx2 = m_screenDistance * w.to.x / w.to.y;
        float vah = m_screenDistance * wallHeight * 0.5f / w.from.y;
        float vbh = m_screenDistance * wallHeight * 0.5f / w.to.y;

        float sx1 = (screen_width() * 0.5f) + (vx1 * screen_width() * 0.5f) / m_aspectRatio;
        float sx2 = (screen_width() * 0.5f) + (vx2 * screen_width() * 0.5f) / m_aspectRatio;
        float sh1 = (vah * screen_height() * 0.5f);
        float sh2 = (vbh * screen_height() * 0.5f);

        int sx1i = (int)std::ceil(sx1);
        int sx2i = (int)std::ceil(sx2);
        float dhdx = (sh2 - sh1) / (sx2 - sx1);
        float h = sh1;

        if (sx1i < 0)
        {
            h += dhdx * -sx1i;
            sx1i = 0;
        }

        if (sx2i > screen_width())
        {
            sx2i = screen_width();
        }

        for (int i = sx1i; i < sx2i; ++i)
        {
            float sy1 = (screen_height() * 0.5f) - h;
            float sy2 = (screen_height() * 0.5f) + h;
            draw_line(i, (int)std::floor(sy1), i, (int)std::floor(sy2), (color + 1) * 3);
            h += dhdx;
        }
    }

    Vertex m_position{};
    float m_screenDistance;
    float m_aspectRatio;
    float m_fovY = DegToRad(90.f);
    float m_heading{};
    float m_t{};
};

boon app;
} // namespace boon

int gli_main(int argc, char** argv)
{
    if (boon::app.initialize("boon", 320, 240, 2))
    {
        boon::app.run();
    }

    return 0;
}
