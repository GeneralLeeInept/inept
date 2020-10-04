#pragma once

#include <gli_core.h>

#include "types.h"
#include "bsp_tree_builder.h"
#include "config.h"

#include <deque>
#include <set>
#include <string>

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

private:
    using StateStack = std::deque<AppState*>;
    StateStack _app_states;

    Config _config{};
};

}
