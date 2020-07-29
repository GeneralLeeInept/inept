#include <gli.h>

#include "vga9.h"

#include <memory>
#include <random>

class Tetris : public gli::App
{
    /*
        https://tetris.wiki/Tetris_Guideline

        "visible playfield 10 blocks wide by 20 blocks tall and a buffer zone above the playfield of 20 rows in height, and the tetrominoes spawn in
        rows 21 and 22 at the bottom of the buffer zone"

        Row 0 is at the bottom of the matrix
    */
    struct V2i
    {
        int x;
        int y;
    };

    using KickTable = V2i[20];

    struct Tetronimo
    {
        uint16_t shape[4];
        const KickTable* kick_table;
    };

    static const int s_playfield_width = 10;
    static const int s_playfield_height = 40;
    static const int s_visible_rows = 20;
    static const int s_buffer_bottom = 21;
    static const int s_tile_size = 16;
    static const int s_border_index = 8;
    static const int s_ghost_index = 9;


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


    char m_playfield[s_playfield_width * s_playfield_height];
    Tetronimo m_tetronimos[7];
    std::unique_ptr<gli::Sprite> m_sprite;
    uint8_t m_bag[16];
    uint8_t m_bag_read_ptr{0xFB};
    uint8_t m_bag_write_ptr{0xFB};
    float m_drop_timer;
    float m_move_timer;
    std::random_device m_random_device;
    std::mt19937 m_random_generator;


    uint16_t generate_tetronimo_shape(const char* desc)
    {
        uint16_t bitmap = 0;

        for (int i = 0; i < 16; ++i)
        {
            bitmap |= (desc[i] == '#') ? (1 << i) : 0;
        }

        return bitmap;
    }


public:
    void refill_bag(bool initial)
    {
        std::uniform_int_distribution<> distribution;

        // add all pieces to the bag
        for (uint8_t i = 0; i < 7; ++i)
        {
            m_bag[(m_bag_write_ptr + i) & 0xF] = i;
        }

        using param_t = std::uniform_int_distribution<>::param_type;

        for (uint8_t i = 6; i > 0; --i)
        {
            uint8_t j = distribution(m_random_generator, param_t(0, i));
            std::swap(m_bag[(m_bag_write_ptr + j) & 0xF], m_bag[(m_bag_write_ptr + i) & 0xF]);
        }

        if (initial)
        {
            while (m_bag[m_bag_read_ptr & 0xF] == 0 || m_bag[m_bag_read_ptr & 0xF] == 3 || m_bag[m_bag_read_ptr & 0xF] == 4 ||
                   m_bag[m_bag_read_ptr & 0xF] == 5)
            {
                uint8_t j = distribution(m_random_generator, param_t(1, 6));
                std::swap(m_bag[m_bag_read_ptr & 0xF], m_bag[(m_bag_read_ptr + j) & 0xF]);
            }
        }

        m_bag_write_ptr += 7;
    }

    uint8_t next_tetronimo()
    {
        uint8_t t = m_bag[(m_bag_read_ptr++) & 0xF];

        if ((uint8_t)(m_bag_write_ptr - m_bag_read_ptr) < 5)
        {
            refill_bag(false);
        }

        return t + 1;
    }

    uint8_t peek_bag(uint8_t index)
    {
        return m_bag[(m_bag_read_ptr + index) & 0xF] + 1;
    }

    bool on_create() override
    {
        memset(m_playfield, 0, sizeof(m_playfield));

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

        //m_random_device
        m_random_generator.seed(m_random_device());
        refill_bag(true);

        m_drop_timer = 0.5f;
        m_move_timer = 0.0f;

        return true;
    }

    void on_destroy() override
    {
    }

    int m_tetronimo = 0;
    int m_tetx = 3;
    int m_tety = s_buffer_bottom + 1;
    int m_tetr = 0;

    bool fits(uint16_t shape, int tx, int ty)
    {
        int by = ty;

        for (int y = 0; y < 4; ++y)
        {
            int bx = tx;

            for (int x = 0; x < 4; ++x)
            {
                int bit = x + y * 4;

                if (shape & (1 << bit))
                {
                    if (bx < 0 || bx >= s_playfield_width || by < 0 || by >= s_playfield_height)
                    {
                        return false;
                    }

                    if (m_playfield[bx + by * s_playfield_width])
                    {
                        return false;
                    }
                }

                bx++;
            }

            by--;
        }

        return true;
    }

    void add_to_playfield(int t, int tx, int ty, int r)
    {
        const Tetronimo* tetronimo = &m_tetronimos[t - 1];
        uint16_t shape = tetronimo->shape[r];
        int by = ty;

        for (int y = 0; y < 4; ++y)
        {
            int bx = tx;

            for (int x = 0; x < 4; ++x)
            {
                int bit = x + y * 4;

                if (shape & (1 << bit))
                {
                    m_playfield[bx + by * s_playfield_width] = t;
                }

                bx++;
            }

            by--;
        }
    }


    V2i playfield_to_screen(const V2i& pf_pos)
    {
        V2i screen_pos;
        screen_pos.x = (pf_pos.x + 1) * s_tile_size;
        screen_pos.y = (s_visible_rows - pf_pos.y - 1) * s_tile_size;
        return screen_pos;
    }

    V2i screen_to_playfield(const V2i& screen_pos)
    {
        V2i pf_pos;
        pf_pos.x = (screen_pos.x / s_tile_size) - 1;
        pf_pos.y = s_visible_rows - (screen_pos.y / s_tile_size) - 1; 
        return pf_pos;
    }

    uint8_t read_playfield(const V2i& pf_pos)
    {
        uint8_t p = 0;

        if  (pf_pos.x >= 0 && pf_pos.x < s_playfield_width && pf_pos.y >= 0 && pf_pos.y < s_playfield_height)
        {
            p = m_playfield[pf_pos.x + pf_pos.y * s_playfield_width];
        }

        return p;
    }

    bool attempt_rotation(int dir)
    {
        int target_r = (m_tetr + dir) & 3;
        const Tetronimo* tetronimo = &m_tetronimos[m_tetronimo - 1];
        const V2i* kick = &(*tetronimo->kick_table)[target_r * 5];
        uint16_t shape = tetronimo->shape[target_r];

        for (int i = 0; i < 5; ++i)
        {
            // Test rotated shape against playfield using kick
            int kicked_x = m_tetx + dir * kick[i].x;
            int kicked_y = m_tety + dir * kick[i].y;

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

    void draw_tetronimo(int x, int y, int t, int r)
    {
        const Tetronimo* tetronimo = &m_tetronimos[t - 1];
        uint16_t shape = tetronimo->shape[r];
        int ox = t * s_tile_size;
        int ty = y;

        for (int y = 0; y < 4; ++y)
        {
            int tx = x;

            for (int x = 0; x < 4; ++x)
            {
                int bit = x + y * 4;

                if (shape & (1 << bit))
                {
                    draw_partial_sprite(tx, ty, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
                }

                tx += s_tile_size;
            }

            ty += s_tile_size;
        }
    }

    bool m_game_over = false;

    bool on_update(float delta) override
    {
        if (m_game_over)
        {
        }
        else
        {
            m_drop_timer -= delta;

            if (m_drop_timer <= 0.0f)
            {
                m_drop_timer += 0.5f;

                if (m_tetronimo)
                {
                    if (!attempt_move(0, -1))
                    {
                        add_to_playfield(m_tetronimo, m_tetx, m_tety, m_tetr);
                        m_tetronimo = 0;
                    }
                }
                else
                {
                    m_tetronimo = next_tetronimo();
                    m_tetx = 3;
                    m_tety = s_buffer_bottom + 1;
                    m_tetr = 0;

                    if (!fits(m_tetronimos[m_tetronimo - 1].shape[m_tetr], m_tetx, m_tety))
                    {
                        m_game_over = true;
                    }
                }
            }

            if (m_tetronimo && !m_game_over)
            {
                if (key_state(gli::Key_A).pressed || (key_state(gli::Key_A).down && m_move_timer <= 0.0f))
                {
                    attempt_move(-1, 0);
                    m_move_timer = 0.2f;
                }
                if (key_state(gli::Key_D).pressed || (key_state(gli::Key_D).down && m_move_timer <= 0.0f))
                {
                    attempt_move(1, 0);
                    m_move_timer = 0.2f;
                }
                if (key_state(gli::Key_Left).pressed || (key_state(gli::Key_Left).down && m_move_timer <= 0.0f))
                {
                    attempt_rotation(-1);
                    m_move_timer = 0.2f;
                }
                if (key_state(gli::Key_Right).pressed || (key_state(gli::Key_Left).down && m_move_timer <= 0.0f))
                {
                    attempt_rotation(1);
                    m_move_timer = 0.2f;
                }
            }
        }

        clear_screen(0);

        {
            int sy = 0;

            for (int y = 0; y < s_visible_rows; ++y)
            {
                int sx = s_tile_size;

                for (int x = 0; x < s_playfield_width; ++x)
                {
                    V2i pf_pos = screen_to_playfield({sx, sy});
                    int ox = read_playfield(pf_pos) * s_tile_size;

                    if (ox >= 0)
                    {
                        draw_partial_sprite(sx, sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
                    }

                    sx += s_tile_size;
                }

                sy += s_tile_size;
            }

            int ox = s_border_index * s_tile_size;
            sy = 0;

            for (int y = 0; y <= s_visible_rows; ++y)
            {
                draw_partial_sprite(0, sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
                draw_partial_sprite(s_tile_size * (s_playfield_width + 1), sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
                sy += s_tile_size;
            }

            int sx = s_tile_size;
            sy = s_tile_size * s_visible_rows;

            for (int x = 0; x < s_playfield_width; ++x)
            {
                draw_partial_sprite(sx, sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
                sx += s_tile_size;
            }
        }

        if (m_tetronimo)
        {
            V2i screen_pos = playfield_to_screen({ m_tetx, m_tety });
            draw_tetronimo(screen_pos.x, screen_pos.y, m_tetronimo, m_tetr);
        }

        for (int i = 0; i < 5; ++i)
        {
            draw_tetronimo(13 * 16, (i * 4 + 1) * 16, peek_bag(i), 0);
        }

        if (m_game_over)
        {
            fill_rect(0, (screen_height() - 32) / 2, screen_width(), 32, 0, gli::Pixel(0xFF000000U), gli::Pixel(0xFF000000U));
            draw_string((screen_width() - 81) / 2, (screen_height() - 16) / 2, "GAME OVER", vga9_glyphs, vga9_glyph_width, vga9_glyph_height, gli::Pixel(0xFFFFFFFFU), gli::Pixel(0xFF000000U));
        }

        return true;
    }
};

Tetris app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Tetris", 18 * 16, 21 * 16, 2))
    {
        app.run();
    }

    return 0;
}
