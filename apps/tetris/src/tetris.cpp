#include "tetris.h"
#include "vga9.h"
#include "gs_play.h"

bool Tetris::on_create()
{
    m_states.resize(GS_COUNT);

    GameStatePtr gs = std::make_unique<GameStatePlay>();

    if (!gs->on_init(this))
    {
        return false;
    }

    m_states[GS_PLAY] = std::move(gs);

    m_gamestate = GS_PLAY;
    m_active_state = m_states[GS_PLAY].get();

    if (!m_active_state->on_enter())
    {
        m_active_state = nullptr;
        return false;
    }

    return true;
}


void Tetris::on_destroy()
{
    if (m_active_state)
    {
        m_active_state->on_exit();
    }

    for (GameStatePtr& ptr : m_states)
    {
        if (ptr)
        {
            ptr->on_destroy();
        }
    }

    m_states.clear();
}


bool Tetris::on_update(float delta)
{
    if (m_active_state)
    {
        if (!m_active_state->on_update(delta))
        {
            m_active_state->on_exit();
            m_active_state = nullptr;
        }
    }

    return true;
}


Tetris app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Tetris", 18 * 16, 21 * 16, 2))
    {
        app.run();
    }

    return 0;
}
