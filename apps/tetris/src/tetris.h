#pragma once

#include <gli.h>

#include <memory>
#include <vector>

class IGameState;

class Tetris : public gli::App
{
    enum GameState
    {
        GS_LOADING,
        GS_ATTRACT,
        GS_MAIN_MENU,
        GS_PLAY,
        GS_COUNT
    };

    using GameStatePtr = std::unique_ptr<IGameState>;

    GameState m_gamestate;
    std::vector<GameStatePtr> m_states;
    IGameState* m_active_state;

public:
    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;
};
