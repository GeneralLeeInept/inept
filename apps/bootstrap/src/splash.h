#pragma once

#include "iappstate.h"

#include <gli.h>

namespace Bootstrap
{

class SplashState : public IAppState
{
public:
    bool on_init(App* app) override;
    void on_destroy() override;
    bool on_enter() override;
    void on_exit() override;
    void on_suspend() override;
    void on_resume() override;
    bool on_update(float delta) override;

private:
    enum Screens
    {
        GliPresents,
        IneptGame,
        OlcCodeJam2020,
        Bootstrap,
        Count
    };

    App* _app{};
    gli::Sprite _splash_images[Screens::Count]{};
    int _current_screen{};
    float _screen_timer{};
};

} // namespace Bootstrap
