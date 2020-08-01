#pragma once

#include <gli.h>

#include <memory>
#include <vector>

class IGameState;

class Tetris : public gli::App
{
public:
    enum GameState
    {
        GS_LOADING,
        GS_ATTRACT,
        GS_FRONTEND,
        GS_PLAY,
        GS_COUNT
    };

    bool on_create() override;
    void on_destroy() override;
    bool on_update(float delta) override;

    void set_next_state(GameState gs);

private:
    using GameStatePtr = std::unique_ptr<IGameState>;

    GameState m_gamestate;
    GameState m_next_gamestate;
    std::vector<GameStatePtr> m_states;
    IGameState* m_active_state;
};
