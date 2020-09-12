#pragma once

#include <gli.h>
#include "appstate.h"
#include "iappstate.h"

#include <unordered_map>

namespace Bootstrap
{

class App : public gli::App
{
public:
    static const uint32_t BgColor = 0xFF1F1D2C;

    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;
    void set_next_state(AppState next_state);

    void draw_text_box(int x, int y, int w, int h, const std::string& text, const gli::Pixel& fg, const gli::Pixel& bg);

private:
    using AppStatePtr = std::unique_ptr<IAppState>;
    std::unordered_map<AppState, AppStatePtr> _states;
    IAppState* _active_state;
    AppState _state;
    AppState _next_state;
    gli::AudioEngine _audio_engine;

    void wrap_text(const std::string& text, int w, size_t& off, size_t& len);
};

} // namespace Bootstrap