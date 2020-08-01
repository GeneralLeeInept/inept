#include "gs_frontend.h"

#include "tetris.h"
#include "vga9.h"


using MenuEntry = std::string;
using Menu = std::vector<MenuEntry>;

enum Menus
{
    M_MAIN,
    M_OPTIONS,
    M_GAME_OPTIONS,
    M_SOUND_OPTIONS,
    M_CONTROLLER_OPTIONS,
    M_QUIT
};

Menu Menus[] = {
    // Main menu
    { "New Game", "Options", "", "Quit" },

    // Options
    { "Game Options", "Sound Options", "Controller Options" },

    // Game Options
    { "Ghost Piece", "Spawn Delay", "Autorepeat Delay" },

    // Sound Options
    { "SFX Volume", "Music Volume" },

    // Controller Options
    { "Controller Type", "Controller Settings" },

    // Quit Menu
    { "No", "Yes" }
};

bool GsFrontend::on_init(Tetris* app)
{
    m_app = app;
    return true;
}


void GsFrontend::on_destroy() {}


bool GsFrontend::on_enter()
{
    m_menu = 0;
    m_selected = 0;
    return true;
}


void GsFrontend::on_exit() {}


void GsFrontend::on_suspend() {}


void GsFrontend::on_resume() {}


bool GsFrontend::on_update(float delta)
{
    bool enter = false;
    bool leave = false;
    bool next = false;
    bool prev = false;

    auto event_handler = [&](gli::KeyEvent& key_event) {
        if (key_event.event == gli::Released && key_event.key == gli::Key_Escape)
        {
            leave = true;
        }
        else if (key_event.event == gli::Released && key_event.key == gli::Key_Enter)
        {
            enter = true;
        }
        else if (key_event.event != gli::Released)
        {
            if (key_event.key == gli::Key_Down)
            {
                next = true;
            }
            else if (key_event.key == gli::Key_Up)
            {
                prev = true;
            }
        }
    };

    m_app->process_key_events(event_handler);

    if (leave)
    {
        if (m_stack.empty())
        {
            m_selected = 3;
        }
        else
        {
            pop_menu();
        }
    }
    else if (enter)
    {
        switch (m_menu)
        {
            case M_MAIN:
            {
                switch (m_selected)
                {
                    case 0:
                    {
                        // New game
                        m_app->set_next_state(Tetris::GS_PLAY);
                        break;
                    }
                    case 1:
                    {
                        // Options
                        push_menu(M_OPTIONS);
                        break;
                    }
                    case 3:
                    {
                        // Quit
                        push_menu(M_QUIT);
                        break;
                    }
                }
                break;
            }
            case M_OPTIONS:
            {
                push_menu(M_GAME_OPTIONS + m_selected);
                break;
            }
            case M_QUIT:
            {
                switch (m_selected)
                {
                    case 0:
                    {
                        pop_menu();
                        break;
                    }
                    case 1:
                    {
                        m_app->quit();
                        break;
                    }
                }
            }
        }
    }
    else if (next && !prev)
    {
        int entries = (int)Menus[m_menu].size();

        do
        {
            if (m_selected < entries - 1)
            {
                m_selected++;
            }
        } while (Menus[m_menu][m_selected][0] == 0);
    }
    else if (prev && !next)
    {
        do
        {
            if (m_selected > 0)
            {
                m_selected--;
            }
        } while (Menus[m_menu][m_selected][0] == 0);
    }

    Menu& current_menu = Menus[m_menu];
    int entries = (int)current_menu.size();
    int h = entries * vga9_glyph_height;
    int y = (m_app->screen_height() - h) / 2;

    m_app->clear_screen(0);

    for (int index = 0; index < entries; ++index)
    {
        const std::string& entry = current_menu[index];
        int w = (int)entry.length() * vga9_glyph_width;
        int x = (m_app->screen_width() - w) / 2;
        m_app->draw_string(x, y, entry.c_str(), vga9_glyphs, vga9_glyph_width, vga9_glyph_height,
                           (index == m_selected) ? gli::Pixel(0xFFFF0000) : gli::Pixel(0xFFFFFFFF), gli::Pixel(0xFF000000));
        y += vga9_glyph_height;
    }

    return true;
}


void GsFrontend::push_menu(int menu)
{
    m_stack.push_back({ m_menu, m_selected });
    m_menu = menu;
    m_selected = 0;
}


void GsFrontend::pop_menu()
{
    m_menu = m_stack.back().first;
    m_selected = m_stack.back().second;
    m_stack.pop_back();
}
