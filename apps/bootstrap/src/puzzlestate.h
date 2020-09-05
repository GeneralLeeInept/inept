#pragma once

#include "iappstate.h"
#include "puzzle.h"

#include <gli.h>

namespace Bootstrap
{

class PuzzleState : public IAppState
{
public:
    bool on_init(App* app) override;
    void on_destroy() override;
    bool on_enter() override;
    void on_exit() override;
    void on_suspend() override;
    void on_resume() override;
    bool on_update(float delta) override;

    void set_puzzle(const Puzzle::Definition& puzzle) { _puzzle = &puzzle; }
    bool result() { return _success; }

private:
    enum Sprite
    {
        Board,
        PiecesSheet,
        GoButtonPressed,
        VerifyingDialog,
        ValidationFailed,
        ValidationSuccess,
        Count
    };

    struct ControlLineDef
    {
        uint8_t line;
        int label;
        int bgtile;
    };

    bool update_simulation(float delta);
    void render(float delta);

    void draw_tile(const ControlLineDef& linedef, int x, int y, bool center);
    size_t select_from_bag(int x, int y);
    size_t select_from_solution(int x, int y);
    void remove_from_solution(int x, int y);
    bool add_to_solution(int x, int y, size_t def);

    std::vector<ControlLineDef> _linedefs;
    std::vector<size_t> _solution;
    gli::Sprite _sprites[Sprite::Count];
    App* _app;
    const Puzzle::Definition* _puzzle;
    size_t _dragging;
    bool _go_pressed;
    float _verifying;
    float _complete;
    bool _success;
    float _state_timer;
    int _state;
};

} // namespace Bootstrap
