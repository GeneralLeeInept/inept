#pragma once

#include <gli_core.h>

#include "types.h"
#include "config.h"
#include "texture_manager.h"

#include <deque>

namespace fist
{

class AppState;

class App : public gli::App
{
public:
    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;

    Config& config();
    TextureManager& texture_manager();

private:
    using StateStack = std::deque<AppState*>;
    StateStack _app_states;

    Config _config{};
    TextureManager _texture_manager{};
};

}
