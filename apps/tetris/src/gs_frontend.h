#pragma once

#include "igamestate.h"

#include <vector>

class GsFrontend : public IGameState
{
public:
    // IGameState
    bool on_init(Tetris* app) override;
    void on_destroy() override;
    bool on_enter() override;
    void on_exit() override;
    void on_suspend() override;
    void on_resume() override;
    bool on_update(float delta) override;

private:
    Tetris* m_app;
    int m_menu;
    int m_selected;

    using StackEntry = std::pair<int, int>;
    std::vector<StackEntry> m_stack;

    void push_menu(int menu);
    void pop_menu();
};
