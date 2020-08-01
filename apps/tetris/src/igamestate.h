#pragma once

class Tetris;

class IGameState
{
public:
    virtual ~IGameState() = default;

    // Called once when gamestate is allocated
    virtual bool on_init(Tetris* app) = 0;

    // Called once before gamestate is freed
    virtual void on_destroy() = 0;

    // Called each time the gamestate is made the active gamestate
    virtual bool on_enter() = 0;

    // Called when the gamestate is no longer the active gamestate
    virtual bool on_exit() = 0;

    // Called when the application is suspended and the gamestate is active
    virtual void on_suspend() = 0;
    
    // Called when the application is resumed and the gamestate is active
    virtual void on_resume() = 0;

    // Called each frame when the gamestate is active
    virtual bool on_update(float delta) = 0;
};
