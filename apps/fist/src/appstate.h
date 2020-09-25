#pragma once

namespace fist
{

class App;
class Event;

class AppState
{
public:
    virtual ~AppState() = default;

    // Called once when app state is allocated
    virtual void on_init(App* app) {}

    // Called once before app state is freed
    virtual void on_destroy() {}

    // Called each time the app state is pushed onto the app state stack
    virtual void on_enter() {}

    // Called when the app state is removed from the app state stack
    virtual void on_exit() {}

    // Called when the application is suspended and the app state is active
    virtual void on_suspend() {}

    // Called when the application is resumed and the app state is active
    virtual void on_resume() {}

    // Called each frame when the gamestate is active
    virtual void on_update(float delta) {}

    // Event handler - return true to consume event
    virtual bool handle_event(Event& ev) { return false; }

protected:
    App* _app;
};

} // namespace Bootstrap
