#include <gli.h>

#include <cmath>

namespace boon
{
struct Vertex
{
    float x;
    float y;
};

 struct Vector
{
    float x;
    float y;
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

static Vertex s_verts[] = { { -1.0f, 2.0f }, { 1.0f, 2.0f } };

class boon : public gli::App
{
public:
    bool on_create() override { return true; }

    void on_destroy() override {}

    bool on_update(float delta) override
    {
        clear_screen(gli::Pixel(0xFF008000));
        m_t += delta;
        m_position.y = std::sin(m_t) * 0.5f;
        m_aspectRatio = (float)screen_width() / (float)screen_height();
        m_screenDistance = 1.0f / std::tan(m_fovY * 0.5f);
        DrawWall(ToViewSpace(s_verts[0]), ToViewSpace(s_verts[1]));
        return true;
    }

private:
    Vertex ToViewSpace(const Vertex& v)
    {
        Vertex vp = v - m_position;
        return vp;
    }

    void DrawWall(const Vertex& a, const Vertex& b)
    {
        float wallHeight = 2.f;
        float vx1 = m_screenDistance * a.x / a.y;
        float vx2 = m_screenDistance * b.x / b.y;
        float vah = m_screenDistance * wallHeight * 0.5f / a.y;
        float vbh = m_screenDistance * wallHeight * 0.5f / b.y;

        float sx1 = (screen_width() * 0.5f) + (vx1 * screen_width() * 0.5f) / m_aspectRatio;
        float sx2 = (screen_width() * 0.5f) + (vx2 * screen_width() * 0.5f) / m_aspectRatio;
        float sh1 = (vah * screen_height() * 0.5f);
        float sh2 = (vbh * screen_height() * 0.5f);

        int sx1i = (int)std::ceil(sx1);
        int sx2i = (int)std::ceil(sx2);
        float dhdx = (sh2 - sh1) / (sx2 - sx1);
        float h = sh1;
        for (int i = sx1i; i < sx2i; ++i)
        {
            float sy1 = (screen_height() * 0.5f) - h;
            float sy2 = (screen_height() * 0.5f) + h;
            draw_line(i, (int)std::floor(sy1), i, (int)std::floor(sy2), 1);
            h += dhdx;
        }
    }

    Vertex m_position{};
    Vector m_heading{ 0.0f, 1.0f };
    float m_screenDistance;
    float m_aspectRatio;
    float m_fovY = DegToRad(90.f);
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
