#include "app.h"

#include "appstate.h"
#include "prototype_state.h"

namespace fist
{

bool App::on_create()
{
    _config.load();

    PrototypeState* state = new PrototypeState;
    state->on_init(this);
    state->on_pushed();
    _app_states.push_front(state);
    
    return true;
}

void App::on_destroy()
{
    while (!_app_states.empty())
    {
        AppState* state = _app_states.front();
        state->on_popped();
        state->on_destroy();
        delete state;
        _app_states.pop_front();
    }

    _config.save();
}

bool App::on_update(float delta)
{
    for (auto& app_state : _app_states)
    {
        app_state->on_update(delta);
    }

    return true;
}

Config& App::config()
{
    return _config;
}

} // namespace fist
