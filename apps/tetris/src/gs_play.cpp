#include "gs_play.h"

#include "tetris.h"
#include "vga9.h"

bool GsPlay::on_init(Tetris* app)
{
    m_app = app;

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

    int screen_tiles_w = m_app->screen_width() / s_tile_size;
    int screen_tiles_h = m_app->screen_height() / s_tile_size;
    m_playfield_screen_min.x = ((screen_tiles_w - s_playfield_width) / 2) * s_tile_size;
    m_playfield_screen_min.y = ((screen_tiles_h - s_visible_rows) / 2) * s_tile_size;
    m_playfield_screen_max.x = m_playfield_screen_min.x + s_playfield_width * s_tile_size;
    m_playfield_screen_max.y = m_playfield_screen_min.y + s_visible_rows * s_tile_size;

    m_random_generator.seed(m_random_device());
    return true;
}


void GsPlay::on_destroy()
{
}


bool GsPlay::on_enter()
{
    refill_bag(true);
    memset(m_playfield, 0, sizeof(m_playfield));
    m_tetronimo = 0;
    m_tetronimo_held = 0;
    m_tetx = 3;
    m_tety = s_buffer_bottom + 1;
    m_tetr = 0;
    m_drop_timer = 0.0f;
    m_move_timer = 0.0f;
    m_game_over = false;
    m_lines = 0;
    m_level = 0;
    m_gamespeed = powf(0.8f - (m_level * 0.007f), m_level);

    return true;
}


void GsPlay::on_exit()
{
}


void GsPlay::on_suspend()
{
}


void GsPlay::on_resume()
{
}


bool GsPlay::on_update(float delta)
{
    if (m_game_over)
    {
        if (m_app->key_state(gli::Key::Key_Escape).released)
        {
            m_app->set_next_state(Tetris::GS_FRONTEND);
        }
    }
    else
    {
        m_drop_timer += delta;

        if (m_drop_timer >= m_gamespeed)
        {
            m_drop_timer -= m_gamespeed;

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
                if (!remove_full_row())
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
        }

        if (m_tetronimo && !m_game_over)
        {
            if (m_move_timer > delta)
            {
                m_move_timer -= delta;
            }
            else
            {
                m_move_timer = 0.0f;
            }

            if (m_app->key_state(gli::Key_Space).pressed)
            {
                hard_drop();
            }
            else if (m_app->key_state(gli::Key_A).pressed || (m_app->key_state(gli::Key_A).down && m_move_timer <= 0.0f))
            {
                attempt_move(-1, 0);
                m_move_timer = 0.2f;
            }
            else if (m_app->key_state(gli::Key_D).pressed || (m_app->key_state(gli::Key_D).down && m_move_timer <= 0.0f))
            {
                attempt_move(1, 0);
                m_move_timer = 0.2f;
            }
            else if (m_app->key_state(gli::Key_Left).pressed || (m_app->key_state(gli::Key_Left).down && m_move_timer <= 0.0f))
            {
                attempt_rotation(-1);
                m_move_timer = 0.2f;
            }
            else if (m_app->key_state(gli::Key_Right).pressed || (m_app->key_state(gli::Key_Right).down && m_move_timer <= 0.0f))
            {
                attempt_rotation(1);
                m_move_timer = 0.2f;
            }
            else if (m_app->key_state(gli::Key_C).pressed)
            {
                hold();
            }
        }
    }

    m_app->clear_screen(0);

    {
        for (int y = 0; y < s_visible_rows; ++y)
        {
            for (int x = 0; x < s_playfield_width; ++x)
            {
                V2i pf_pos{ x, y };
                V2i tile_pos = playfield_to_screen(pf_pos);
                int ox = read_playfield(pf_pos) * s_tile_size;

                if (ox >= 0)
                {
                    m_app->draw_partial_sprite(tile_pos.x, tile_pos.y, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
                }
            }
        }

        int ox = s_border_index * s_tile_size;
        int sy = m_playfield_screen_min.y;

        for (int y = 0; y < s_visible_rows; ++y)
        {
            m_app->draw_partial_sprite(m_playfield_screen_min.x - s_tile_size, sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
            m_app->draw_partial_sprite(m_playfield_screen_max.x, sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
            sy += s_tile_size;
        }

        int sx = m_playfield_screen_min.x - s_tile_size;

        for (int x = 0; x < s_playfield_width + 2; ++x)
        {
            m_app->draw_partial_sprite(sx, sy, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
            sx += s_tile_size;
        }
    }

    if (m_tetronimo)
    {
        V2i screen_pos;

        // draw ghost
        uint16_t shape = m_tetronimos[m_tetronimo - 1].shape[m_tetr];
        int ghost_y = m_tety;

        while (fits(shape, m_tetx, ghost_y - 1))
        {
            ghost_y = ghost_y - 1;
        }

        if (ghost_y != m_tety)
        {
            screen_pos = playfield_to_screen({ m_tetx, ghost_y });
            draw_tetronimo(screen_pos.x, screen_pos.y, m_tetronimo, m_tetr, true, true);
        }

        screen_pos = playfield_to_screen({ m_tetx, m_tety });
        draw_tetronimo(screen_pos.x, screen_pos.y, m_tetronimo, m_tetr, true, false);
    }

    for (int i = 0; i < 5; ++i)
    {
        draw_tetronimo(m_playfield_screen_max.x + s_tile_size * 3, m_playfield_screen_min.y + (i * 4) * s_tile_size, peek_bag(i), 0, false, (i > 0));
    }

    if (m_tetronimo_held)
    {
        draw_tetronimo(m_playfield_screen_min.x - s_tile_size * 7, m_playfield_screen_min.y, m_tetronimo_held, 0, false, false);
    }

    m_app->draw_string(m_playfield_screen_min.x - s_tile_size * 9, m_playfield_screen_max.y - 64, "LEVEL:", vga9_glyphs, vga9_glyph_width,
                       vga9_glyph_height, gli::Pixel(0xFFFFFFFF), gli::Pixel(0xFF000000));
    m_app->format_string(m_playfield_screen_min.x - s_tile_size * 9, m_playfield_screen_max.y - 48, vga9_glyphs, vga9_glyph_width, vga9_glyph_height,
                       gli::Pixel(0xFFFFFFFF), gli::Pixel(0xFF000000), "%-5d", m_level + 1);
    m_app->draw_string(m_playfield_screen_min.x - s_tile_size * 9, m_playfield_screen_max.y - 32, "LINES:", vga9_glyphs, vga9_glyph_width,
                       vga9_glyph_height, gli::Pixel(0xFFFFFFFF), gli::Pixel(0xFF000000));
    m_app->format_string(m_playfield_screen_min.x - s_tile_size * 9, m_playfield_screen_max.y - 16, vga9_glyphs, vga9_glyph_width, vga9_glyph_height,
                         gli::Pixel(0xFFFFFFFF), gli::Pixel(0xFF000000), "%d", m_lines);

    if (m_game_over)
    {
        m_app->fill_rect(0, (m_app->screen_height() - 32) / 2, m_app->screen_width(), 32, 0, gli::Pixel(0xFF000000U), gli::Pixel(0xFF000000U));
        m_app->draw_string((m_app->screen_width() - 81) / 2, (m_app->screen_height() - 16) / 2, "GAME OVER", vga9_glyphs, vga9_glyph_width,
            vga9_glyph_height, gli::Pixel(0xFFFFFFFFU), gli::Pixel(0xFF000000U));
    }

    return true;
}


uint16_t GsPlay::generate_tetronimo_shape(const char* desc)
{
    uint16_t bitmap = 0;

    for (int i = 0; i < 16; ++i)
    {
        bitmap |= (desc[i] == '#') ? (1 << i) : 0;
    }

    return bitmap;
}


void GsPlay::refill_bag(bool initial)
{
    std::uniform_int_distribution<> distribution;

    if (initial)
    {
        // Reset bag
        m_bag_write_ptr = 0;
        m_bag_read_ptr = 0;
    }

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


uint8_t GsPlay::next_tetronimo()
{
    uint8_t t = m_bag[(m_bag_read_ptr++) & 0xF];

    if ((uint8_t)(m_bag_write_ptr - m_bag_read_ptr) < 5)
    {
        refill_bag(false);
    }

    return t + 1;
}


uint8_t GsPlay::peek_bag(uint8_t index)
{
    return m_bag[(m_bag_read_ptr + index) & 0xF] + 1;
}


bool GsPlay::fits(uint16_t shape, int tx, int ty)
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


void GsPlay::add_to_playfield(int t, int tx, int ty, int r)
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


bool GsPlay::remove_full_row()
{
    int full_rows = 0;

    for (int i = 0; i < s_playfield_height; ++i)
    {
        bool full = true;

        for (int x = 0; x < s_playfield_width; ++x)
        {
            if (read_playfield({ x, i }) == 0)
            {
                full = false;
                break;
            }
        }

        if (full)
        {
            for (int j = i; j < s_playfield_height; ++j)
            {
                memcpy(&m_playfield[j * s_playfield_width], &m_playfield[(j + 1) * s_playfield_width], s_playfield_width * sizeof(m_playfield[0]));
            }
            memset(&m_playfield[(s_playfield_height - 1) * s_playfield_width], 0, s_playfield_width * sizeof(m_playfield[0]));
            m_lines++;
            m_level = m_lines / 10;
            m_gamespeed = powf(0.8f - (m_level * 0.007f), m_level);
            return true;
        }
    }

    return false;
}


V2i GsPlay::playfield_to_screen(const V2i& pf_pos)
{
    V2i screen_pos;
    screen_pos.x = pf_pos.x * s_tile_size + m_playfield_screen_min.x;
    screen_pos.y = (s_visible_rows - 1 - pf_pos.y) * s_tile_size + m_playfield_screen_min.y;
    return screen_pos;
}


V2i GsPlay::screen_to_playfield(const V2i& screen_pos)
{
    V2i pf_pos;
    pf_pos.x = (screen_pos.x - m_playfield_screen_min.x) / s_tile_size;
    pf_pos.y = ((m_playfield_screen_min.y - screen_pos.y) / s_tile_size) + (s_visible_rows - 1);
    return pf_pos;
}


uint8_t GsPlay::read_playfield(const V2i& pf_pos)
{
    uint8_t p = 0;

    if (pf_pos.x >= 0 && pf_pos.x < s_playfield_width && pf_pos.y >= 0 && pf_pos.y < s_playfield_height)
    {
        p = m_playfield[pf_pos.x + pf_pos.y * s_playfield_width];
    }

    return p;
}


bool GsPlay::attempt_rotation(int dir)
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


bool GsPlay::attempt_move(int dx, int dy)
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


void GsPlay::hard_drop()
{
    const Tetronimo* tetronimo = &m_tetronimos[m_tetronimo - 1];
    uint16_t shape = tetronimo->shape[m_tetr];

    while (fits(shape, m_tetx, m_tety - 1))
    {
        m_tety -= 1;
    }

    add_to_playfield(m_tetronimo, m_tetx, m_tety, m_tetr);
}


void GsPlay::draw_tetronimo(int x, int y, int t, int r, bool clip_to_payfield, bool ghost)
{
    const Tetronimo* tetronimo = &m_tetronimos[t - 1];
    uint16_t shape = tetronimo->shape[r];
    int ox = (ghost ? s_ghost_index : t) * s_tile_size;
    int ty = y;

    for (int y = 0; y < 4; ++y, ty += s_tile_size)
    {
        if (clip_to_payfield && (ty < m_playfield_screen_min.y || ty >= m_playfield_screen_max.y))
        {
            continue;
        }

        int tx = x;

        for (int x = 0; x < 4; ++x, tx += s_tile_size)
        {
            if (clip_to_payfield && (tx < m_playfield_screen_min.x || tx >= m_playfield_screen_max.x))
            {
                continue;
            }

            int bit = x + y * 4;

            if (shape & (1 << bit))
            {
                m_app->draw_partial_sprite(tx, ty, m_sprite.get(), ox, 0, s_tile_size, s_tile_size);
            }
        }
    }
}

void GsPlay::hold()
{
    std::swap(m_tetronimo, m_tetronimo_held);
    m_tetx = 3;
    m_tety = s_buffer_bottom + 1;
    m_tetr = 0;
}
