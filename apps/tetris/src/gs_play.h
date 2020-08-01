#pragma once

#include "igamestate.h"

#include "gli.h"

#include <memory>
#include <random>

struct V2i
{
    int x;
    int y;
};


class GameStatePlay : public IGameState
{
public:
    // IGameState
    bool on_init(Tetris* app) override;
    void on_destroy() override;
    bool on_enter() override;
    bool on_exit() override;
    void on_suspend() override;
    void on_resume() override;
    bool on_update(float delta) override;

private:
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
        { 0, 0 },
        { -1, 0 },
        { -1, 1 },
        { 0, -2 },
        { -1, -2 },

        // 1 -> 2
        { 0, 0 },
        { 1, 0 },
        { 1, -1 },
        { 0, 2 },
        { 1, 2 },

        // 2 -> 3
        { 0, 0 },
        { 1, 0 },
        { 1, 1 },
        { 0, -2 },
        { 1, -2 },

        // 3 -> 0
        { 0, 0 },
        { -1, 0 },
        { -1, -1 },
        { 0, 2 },
        { -1, 2 },
    };


    static constexpr KickTable kick_table_i = {
        // 0 -> 1
        { 0, 0 },
        { -2, 0 },
        { 1, 0 },
        { -2, -1 },
        { 1, 2 },

        // 1 -> 2
        { 0, 0 },
        { -1, 0 },
        { 2, 0 },
        { -1, 2 },
        { 2, -1 },

        // 2 -> 3
        { 0, 0 },
        { 2, 0 },
        { -1, 0 },
        { 2, 1 },
        { -1, -2 },

        // 3 -> 0
        { 0, 0 },
        { 1, 0 },
        { -2, 0 },
        { 1, -2 },
        { -2, 1 },
    };


    static constexpr KickTable kick_table_o = {
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    };


    Tetris* m_app;
    char m_playfield[s_playfield_width * s_playfield_height];
    Tetronimo m_tetronimos[7];
    std::unique_ptr<gli::Sprite> m_sprite;
    uint8_t m_bag[16];
    uint8_t m_bag_read_ptr{ 0xFB };
    uint8_t m_bag_write_ptr{ 0xFB };
    float m_drop_timer;
    float m_move_timer;
    std::random_device m_random_device;
    std::mt19937 m_random_generator;
    int m_tetronimo;
    int m_tetx;
    int m_tety;
    int m_tetr;
    bool m_game_over = false;


    uint16_t generate_tetronimo_shape(const char* desc);
    void refill_bag(bool initial);
    uint8_t next_tetronimo();
    uint8_t peek_bag(uint8_t index);
    bool fits(uint16_t shape, int tx, int ty);
    void add_to_playfield(int t, int tx, int ty, int r);
    bool remove_full_row();
    V2i playfield_to_screen(const V2i& pf_pos);
    V2i screen_to_playfield(const V2i& screen_pos);
    uint8_t read_playfield(const V2i& pf_pos);
    bool attempt_rotation(int dir);
    bool attempt_move(int dx, int dy);
    void draw_tetronimo(int x, int y, int t, int r, bool ghost);
};
