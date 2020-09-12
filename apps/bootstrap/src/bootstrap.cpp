#include "bootstrap.h"

#include "appstate.h"
#include "gameplaystate.h"
#include "iappstate.h"
#include "splash.h"
#include "version.h"
#include "vga9.h"

#include <memory>
#include <string>


#include "tilemap.h"

namespace Bootstrap
{

bool App::on_create()
{
    if (!_audio_engine.init())
    {
        return false;
    }

    _states.reserve(AppState::Count);
    _states.insert(std::pair<AppState, AppStatePtr>(AppState::Splash, std::make_unique<SplashState>()));
    //_states.insert(std::pair<AppState, AppStatePtr>(AppState::Frontend, std::make_unique<TitleScreenState>()));
    _states.insert(std::pair<AppState, AppStatePtr>(AppState::InGame, std::make_unique<GamePlayState>()));

    for (const auto& kvp : _states)
    {
        if (kvp.second)
        {
            if (!kvp.second->on_init(this))
            {
                return false;
            }
        }
    }

    _state = AppState::Count;
    _next_state = AppState::Splash;
    _active_state = nullptr;

    if (!_audio_engine.start())
    {
        return false;
    }

    return true;
}

void App::on_destroy()
{
    if (_active_state)
    {
        _active_state->on_exit();
        _active_state = nullptr;
    }

    for (const auto& kvp : _states)
    {
        if (kvp.second)
        {
            kvp.second->on_destroy();
        }
    }

    _states.clear();
    _audio_engine.stop();
}

bool App::on_update(float delta)
{
    if (_next_state != AppState::Count)
    {
        if (_active_state)
        {
            _active_state->on_exit();
            _active_state = nullptr;
        }

        const auto iter = _states.find(_next_state);

        if (iter != _states.end())
        {
            _active_state = iter->second.get();
        }

        if (_active_state)
        {
            if (!_active_state->on_enter())
            {
                _active_state = nullptr;
                return false;
            }
        }

        _next_state = AppState::Count;
        delta = 0.0f;
    }

    if (_active_state)
    {
        if (!_active_state->on_update(delta))
        {
            _active_state->on_exit();
            _active_state = nullptr;
        }
    }

    return !!_active_state;
}


void App::set_next_state(AppState next_state)
{
    _next_state = next_state;
}


void App::wrap_text(const std::string& text, int w, size_t& off, size_t& len)
{
    if (off)
    {
        while (text[off] == ' ')
            ++off;
    }

    size_t space = 0;

    for (size_t pos = off; pos < text.size(); ++pos)
    {
        if ((pos - off) >= w && space > 0)
        {
            len = space - off;
            return;
        }

        if (text[pos] == ' ')
        {
            space = pos;
        }
    }

    len = std::string::npos;
}


void App::draw_text_box(int x, int y, int w, int h, const std::string& text, const gli::Pixel& fg, const gli::Pixel& bg)
{
    int line_width_chars = w / vga9_glyph_width;
    int max_lines = h / vga9_glyph_height;

    size_t pos = 0;
    size_t len = 0;

    for (int i = 0; i < max_lines && pos < text.size() && len != std::string::npos; ++i)
    {
        wrap_text(text, line_width_chars, pos, len);
        draw_string(x, y, text.substr(pos, len).c_str(), vga9_glyphs, vga9_glyph_width, vga9_glyph_height, fg, bg);
        pos += len;
        y += vga9_glyph_height;
    }
}


} // namespace Bootstrap

Bootstrap::App app;

#ifdef WIN32
#define GLI_PLATFORM "Win32"
#else
#define GLI_PLATFORM "x64"
#endif

int gli_main(int argc, char** argv)
{
    size_t size = std::snprintf(nullptr, 0, "Bootstrap (%s %s)", GLI_PLATFORM, APP_VERSION) + 1;
    std::string title(size, '\0');
    std::snprintf(&title[0], size, "Bootstrap (%s %s)", GLI_PLATFORM, APP_VERSION);

    if (app.initialize(title.c_str(), 640, 360, 2))
    {
        app.run();
    }

    return 0;
}
