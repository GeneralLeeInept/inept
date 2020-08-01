#include "tetris.h"

#include "vga9.h"
#include "gs_frontend.h"
#include "gs_play.h"

bool Tetris::on_create()
{
    m_states.resize(GS_COUNT);

    GameStatePtr gs = std::make_unique<GsPlay>();

    if (!gs->on_init(this))
    {
        return false;
    }

    m_states[GS_PLAY] = std::move(gs);


    gs = std::make_unique<GsFrontend>();

    if (!gs->on_init(this))
    {
        return false;
    }

    m_states[GS_FRONTEND] = std::move(gs);

    m_next_gamestate = GS_FRONTEND;
    m_gamestate = GS_COUNT;
    m_active_state = nullptr;

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
    if (m_next_gamestate != GS_COUNT)
    {
        m_gamestate = m_next_gamestate;
        m_next_gamestate = GS_COUNT;

        if (m_active_state)
        {
            m_active_state->on_exit();
        }

        m_active_state = m_states[m_gamestate].get();
        
        if (m_active_state)
        {
            if (!m_active_state->on_enter())
            {
                m_active_state = nullptr;
                return false;
            }
        }
    }

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


void Tetris::set_next_state(GameState gs)
{
    m_next_gamestate = gs;
}


Tetris app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Tetris", 30 * 16, 24 * 16, 2))
    {
        app.run();
    }

    return 0;
}
