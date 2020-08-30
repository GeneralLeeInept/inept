#include "splash.h"

#include "bootstrap.h"

#include <cmath>

#if GLI_SHIPPING
#define GliMediaPath(p) R"(//assets.glp//)" p
#else
#define GliMediaPath(p) p
#endif

namespace Bootstrap
{

static constexpr float fade_time()
{
    return 0.5f;
}

static constexpr float screen_time()
{
    return 2.0f;
}

static float ease_in(float t)
{
    return t <= 0.0f ? 0.0f : (t >= 1.0f ? 1.0f : 1.0f - std::pow(1.0f - t, 4.0f));
}

static float ease_out(float t)
{
    return 1.0f - (t <= 0.0f ? 0.0f : (t >= 1.0f ? 1.0f : 1.0f - std::pow(t, 4.0f)));
}

bool SplashState::on_init(App* app)
{
    _app = app;
    return true;
}

void SplashState::on_destroy() {}

bool SplashState::on_enter()
{
    // Load images now - would like to be able to do this async
    static const char* filenames[Screens::Count] = { GliMediaPath("frontend/gli_presents.png"), GliMediaPath("frontend/an_inept_game.png"),
                                                     GliMediaPath("frontend/olc_codejam.png"), GliMediaPath("frontend/bootstrap.png") };

    for (int i = 0; i < Screens::Count; ++i)
    {
        if (!_splash_images[i].load(filenames[i]))
        {
            return false;
        }
    }

    _current_screen = Screens::GliPresents;
    _screen_timer = -1.0f;

    return true;
}

void SplashState::on_exit()
{
    for (auto& image : _splash_images)
    {
        image.unload();
    }
}

void SplashState::on_suspend() {}

void SplashState::on_resume() {}

bool SplashState::on_update(float delta)
{
    float alpha = 1.0f;

    _screen_timer += delta;

    if (_screen_timer < fade_time())
    {
        // Fading in
        alpha = ease_in(_screen_timer / fade_time());
    }
    else if (_screen_timer < screen_time() - fade_time())
    {
        // Not interesting
    }
    else if (_screen_timer < screen_time())
    {
        // Fading out
        alpha = ease_out((screen_time() - _screen_timer) / fade_time());
    }
    else
    {
        // Next
        _current_screen++;
        _screen_timer = 0.0f;
        alpha = 0.0f;
    }

    _app->clear_screen(gli::Pixel(16, 0, 22));

    if (_screen_timer >= 0.0f)
    {
        if (_current_screen < Screens::Count)
        {
            _app->clear_screen(gli::Pixel(16, 0, 22));
            int sw = _splash_images[_current_screen].width();
            int sh = _splash_images[_current_screen].height();
            int x = (_app->screen_width() - sw) / 2;
            int y = (_app->screen_height() - sh) / 2;
            _app->blend_sprite(x, y, _splash_images[_current_screen], (uint8_t)(alpha * 255.0f));
        }
        else
        {
            //_app->set_next_state(AppState::Frontend);
            _current_screen = 0;
        }
    }

    return true;
}

} // namespace Bootstrap