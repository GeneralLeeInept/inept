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
    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;
    void set_next_state(AppState next_state);

private:
    using AppStatePtr = std::unique_ptr<IAppState>;
    std::unordered_map<AppState, AppStatePtr> _states;
    IAppState* _active_state;
    AppState _state;
    AppState _next_state;
};

} // namespace Bootstrap