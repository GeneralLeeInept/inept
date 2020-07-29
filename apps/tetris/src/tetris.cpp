#include <gli.h>

#include "vga9.h"

#include <memory>

class Tetris : public gli::App
{
    /*
        https://tetris.wiki/Tetris_Guideline

        visible playfield 10 blocks wide by 20 blocks tall and a buffer zone above the playfield of 20 rows in height, and the tetrominoes spawn in
        rows 21 and 22 at the bottom of the buffer zone
    */
    char m_playfield[41 * 12];

    struct v2i
    {
        int x;
        int y;
    };

    using KickTable = v2i[20];

    struct Tetronimo
    {
        uint16_t shape[4];
        const KickTable* kick_table;
    };

    Tetronimo m_tetronimos[7];

    static constexpr KickTable kick_table_jlstz = {
        // 0 -> 1
        { 0, 0 }, { -1, 0 }, { -1, 1 }, { 0, -2 }, { -1, -2 }, 

        // 1 -> 2
        { 0, 0 }, { 1, 0 }, { 1, -1 }, { 0, 2 }, { 1, 2 }, 

        // 2 -> 3
        { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, -2 }, { 1, -2 }, 

        // 3 -> 0
        { 0, 0 }, { -1, 0 }, { -1, -1 }, { 0, 2 }, { -1, 2 }, 
    };

    static constexpr KickTable kick_table_i = {
        // 0 -> 1
        { 0, 0 }, { -2, 0 }, { 1, 0 }, { -2, -1 }, { 1, 2 }, 

        // 1 -> 2
        { 0, 0 }, { -1, 0 }, { 2, 0 }, { -1, 2 }, { 2, -1 }, 

        // 2 -> 3
        { 0, 0 }, { 2, 0 }, { -1, 0 }, { 2, 1 }, { -1, -2 }, 

        // 3 -> 0
        { 0, 0 }, { 1, 0 }, { -2, 0 }, { 1, -2 }, { -2, 1 }, 
    };

    static constexpr KickTable kick_table_o = {
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, 
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, 
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, 
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, 
    };

    uint16_t generate_tetronimo_shape(const char* desc)
    {
        uint16_t bitmap = 0;

        for (int i = 0; i < 16; ++i)
        {
            bitmap |= (desc[i] == '#') ? (1 << i) : 0;
        }

        return bitmap;
    }

    std::unique_ptr<gli::Sprite> m_sprite;

public:
    bool on_create() override
    {
        memset(m_playfield, 0, sizeof(m_playfield));

        for (int y = 0; y < 41; ++y)
        {
            m_playfield[y * 12] = 8;
            m_playfield[11 + y * 12] = 8;
        }

        for (int x = 0; x < 12; ++x)
        {
            m_playfield[x + 40 * 12] = 8;
        }

        // J
        m_tetronimos[1].shape[0] = generate_tetronimo_shape(
                "#..."
                "###."
                "...."
                "....");
        m_tetronimos[1].shape[1] = generate_tetronimo_shape(
                ".##."
                ".#.."
                ".#.."
                "....");
        m_tetronimos[1].shape[2] = generate_tetronimo_shape(
                "...."
                "###."
                "..#."
                "....");
        m_tetronimos[1].shape[3] = generate_tetronimo_shape(
                ".#.."
                ".#.."
                "##.."
                "....");
        m_tetronimos[1].kick_table = &kick_table_jlstz;

        // L
        m_tetronimos[2].shape[0] = generate_tetronimo_shape(
                "..#."
                "###."
                "...."
                "....");
        m_tetronimos[2].shape[1] = generate_tetronimo_shape(
                ".#.."
                ".#.."
                ".##."
                "....");
        m_tetronimos[2].shape[2] = generate_tetronimo_shape(
                "...."
                "###."
                "#..."
                "....");
        m_tetronimos[2].shape[3] = generate_tetronimo_shape(
                "##.."
                ".#.."
                ".#.."
                "....");
        m_tetronimos[2].kick_table = &kick_table_jlstz;

        // S
        m_tetronimos[4].shape[0] = generate_tetronimo_shape(
                ".##."
                "##.."
                "...."
                "....");
        m_tetronimos[4].shape[1] = generate_tetronimo_shape(
                ".#.."
                ".##."
                "..#."
                "....");
        m_tetronimos[4].shape[2] = generate_tetronimo_shape(
                "...."
                ".##."
                "##.."
                "....");
        m_tetronimos[4].shape[3] = generate_tetronimo_shape(
                "#..."
                "##.."
                ".#.."
                "....");
        m_tetronimos[4].kick_table = &kick_table_jlstz;

        // T
        m_tetronimos[6].shape[0] = generate_tetronimo_shape(
                ".#.."
                "###."
                "...."
                "....");
        m_tetronimos[6].shape[1] = generate_tetronimo_shape(
                ".#.."
                ".##."
                ".#.."
                "....");
        m_tetronimos[6].shape[2] = generate_tetronimo_shape(
                "...."
                "###."
                ".#.."
                "....");
        m_tetronimos[6].shape[3] = generate_tetronimo_shape(
                ".#.."
                "##.."
                ".#.."
                "....");
        m_tetronimos[6].kick_table = &kick_table_jlstz;

        // Z
        m_tetronimos[5].shape[0] = generate_tetronimo_shape(
                "##.."
                ".##."
                "...."
                "....");
        m_tetronimos[5].shape[1] = generate_tetronimo_shape(
                "..#."
                ".##."
                ".#.."
                "....");
        m_tetronimos[5].shape[2] = generate_tetronimo_shape(
                "...."
                "##.."
                ".##."
                "....");
        m_tetronimos[5].shape[3] = generate_tetronimo_shape(
                ".#.."
                "##.."
                "#..."
                "....");
        m_tetronimos[5].kick_table = &kick_table_jlstz;

        // O
        m_tetronimos[3].shape[0] = generate_tetronimo_shape(
                ".##."
                ".##."
                "...."
                "....");
        m_tetronimos[3].shape[1] = generate_tetronimo_shape(
                ".##."
                ".##."
                "...."
                "....");
        m_tetronimos[3].shape[2] = generate_tetronimo_shape(
                ".##."
                ".##."
                "...."
                "....");
        m_tetronimos[3].shape[3] = generate_tetronimo_shape(
                ".##."
                ".##."
                "...."
                "....");
        m_tetronimos[3].kick_table = &kick_table_o;

        // I
        m_tetronimos[0].shape[0] = generate_tetronimo_shape(
                "...."
                "####"
                "...."
                "....");
        m_tetronimos[0].shape[1] = generate_tetronimo_shape(
                "..#."
                "..#."
                "..#."
                "..#.");
        m_tetronimos[0].shape[2] = generate_tetronimo_shape(
                "...."
                "...."
                "####"
                "....");
        m_tetronimos[0].shape[3] = generate_tetronimo_shape(
                ".#.."
                ".#.."
                ".#.."
                ".#..");
        m_tetronimos[0].kick_table = &kick_table_i;

        m_sprite = std::make_unique<gli::Sprite>();

        if (!m_sprite->load("res/tetronimos.png"))
        {
            return false;
        }

        return true;
    }

    void on_destroy() override
    {
    }

    int m_drag_color = 0;
    int m_drag_button = -1;
    int m_tetronimo = 0;
    int m_tetx = 4;
    int m_tety = 22;
    int m_tetr = 0;

    bool fits(uint16_t shape, int tx, int ty)
    {
        if (tx < 0 || ty < 0 || tx > 12 || ty > 41)
        {
            return false;
        }

        int by = ty;

        for (int y = 0; y < 4; ++y)
        {
            int bx = tx;

            for (int x = 0; x < 4; ++x)
            {
                int bit = x + y * 4;

                if (shape & (1 << bit))
                {
                    if (m_playfield[bx + by * 12])
                    {
                        return false;
                    }
                }

                bx++;
            }

            by++;
        }

        return true;
    }

    bool attempt_rotation(int dir)
    {
        int target_r = (m_tetr + dir) & 3;
        const Tetronimo* tetronimo = &m_tetronimos[m_tetronimo - 1];
        const v2i* kick = &(*tetronimo->kick_table)[target_r * 5];
        uint16_t shape = tetronimo->shape[target_r];

        for (int i = 0; i < 5; ++i)
        {
            // Test rotated shape against playfield using kick
            int kicked_x = m_tetx + dir * kick[i].x;
            int kicked_y = m_tety - dir * kick[i].y;

            if (fits(shape, kicked_x, kicked_y))
            {
                m_tetx = kicked_x;
                m_tety = kicked_y;
                m_tetr = target_r;
                return true;
            }
        }

        return false;
    }
    
    bool attempt_move(int dx, int dy)
    {
        const Tetronimo* tetronimo = &m_tetronimos[m_tetronimo - 1];
        uint16_t shape = tetronimo->shape[m_tetr];

        if (fits(shape, m_tetx + dx, m_tety + dy))
        {
            m_tetx += dx;
            m_tety += dy;
            return true;
        }

        return false;
    }

    bool on_update(float delta) override
    {
        MouseState ms = mouse_state();

        int tx = (ms.x / 16);
        int ty = (ms.y / 16) + 20;

        if (tx > 0 && tx < 11 && ty < 40)
        {
            if (m_drag_button < 0)
            {
                if (ms.buttons[0].pressed)
                {
                    m_drag_color = (m_playfield[tx + (ty * 12)] + 1) & 7;
                    m_drag_button = 0;
                }
                else if (ms.buttons[1].pressed)
                {
                    m_drag_color = 0;
                    m_drag_button = 1;
                }
                else if (ms.buttons[2].pressed)
                {
                    m_drag_color = (m_playfield[tx + (ty * 12)] - 1) & 7;
                    m_drag_button = 2;
                }
            }
            else
            {
                if (!ms.buttons[m_drag_button].down)
                {
                    m_drag_button = -1;
                }
            }

            if (m_drag_button >= 0)
            {
                m_playfield[tx + (ty * 12)] = m_drag_color;
            }
        }

        if (key_state(gli::Key_Space).pressed)
        {
            m_tetronimo = (m_tetronimo + 1) & 7;
        }

        if (m_tetronimo)
        {
            if (key_state(gli::Key_A).pressed)
            {
                attempt_move(-1, 0);
            }
            if (key_state(gli::Key_D).pressed)
            {
                attempt_move(1, 0);
            }
            if (key_state(gli::Key_W).pressed)
            {
                attempt_move(0, -1);
            }
            if (key_state(gli::Key_S).pressed)
            {
                attempt_move(0, 1);
            }
            if (key_state(gli::Key_Left).pressed)
            {
                attempt_rotation(-1);
            }
            if (key_state(gli::Key_Right).pressed)
            {
                attempt_rotation(1);
            }
        }

        clear_screen(0);

        {
            int ty = 0;

            for (int y = 20; y < 41; ++y)
            {
                int tx = 0;

                for (int x = 0; x < 12; ++x)
                {
                    int ox = (m_playfield[x + y * 12] - 1) * 16;

                    if (ox >= 0)
                    {
                        draw_partial_sprite(tx, ty, m_sprite.get(), ox, 0, 16, 16);
                    }

                    tx += 16;
                }

                ty += 16;
            }
        }

        if (m_tetronimo)
        {
            const Tetronimo* tetronimo = &m_tetronimos[m_tetronimo - 1];
            uint16_t shape = tetronimo->shape[m_tetr];
            int ox = (m_tetronimo - 1) * 16;
            int ty = (m_tety - 20) * 16;

            for (int y = 0; y < 4; ++y)
            {
                int tx = m_tetx * 16;

                for (int x = 0; x < 4; ++x)
                {
                    int bit = x + y * 4;

                    if (shape & (1 << bit))
                    {
                        draw_partial_sprite(tx, ty, m_sprite.get(), ox, 0, 16, 16);
                    }

                    tx += 16;
                }

                ty += 16;
            }
        }

        return true;
    }
};

Tetris app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Tetris", 12 * 16, 21 * 16, 2))
    {
        app.run();
    }

    return 0;
}
