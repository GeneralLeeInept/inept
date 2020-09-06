#include "puzzlestate.h"

#include "assets.h"
#include "bootstrap.h"
#include "collision.h"
#include "puzzle.h"
#include "random.h"
#include "types.h"
#include "vga9.h"

#include <algorithm>
#include <numeric>

namespace Bootstrap
{

enum PuzzleTileSheet
{
    bAssert_d,
    bAssert_i,
    bLatch_d,
    bLatch_i,
    bSet,
    bLatch_x,
    lblAC,
    lblX,
    lblPCL,
    lblPCH,
    lblABL,
    lblABH,
    lblAI,
    lblBI,
    lblADD,
    lblDL,
    lblDOR,
    lblP,
    lblPI,
    lblW,
    lblSUM,
    lblSHR,
    lblOR,
    lblXOR,
    lblAND,
    lblCI,
    lblNEG,
    lblAZ,
    lblBZ,
    lblC,
    Ghost
};

enum TileSheetLayout
{
    TileSize = 30,
    Columns = 10
};

enum PuzzleBoardLayout
{
    SolutionX = 49,
    SolutionY = 17,
    SolutionColumns = 18,
    SolutionRows = 4,
    SolutionStep = 32,
    BagX = 208,
    BagY = 152,
    BagStep = 32,
    BagColumns = 7,
    BagRows = 6,
    OpcodeTitleX = 34,
    OpcodeTitleY = 165,
    OpcodeTitleW = 154,
    OpcodeTitleH = 16,
    OpcodeDescX = 34,
    OpcodeDescY = 181,
    OpcodeDescW = 154,
    OpcodeDescH = 99,
    GoButtonX = 31,
    GoButtonY = 301,
    GoButtonW = 160,
    GoButtonH = 32,
    ToolTipX = 452,
    ToolTipY = 159,
    ToolTipW = 156,
    ToolTipH = 165,
    TextColor = 0xFFA25500,
    TextBgColor = 0xFF280920,
    BgColor = 0xFFC4C4C4
};


static const std::string ToolTips[] = {
    "Latch the accumulator from the internal bus.",
    "Latch the index register from the internal bus.",
    "Latch the program counter low byte from the internal bus.",
    "Latch the program counter high byte from the internal bus.",
    "Latch the address bus register low byte from the internal bus.",
    "Latch the address bus register high byte from the internal bus.",
    "Latch the ALU input A register from the internal bus.",
    "Latch the ALU input B register from the internal bus.",
    "Latch the ALU output register from the current output of the ALU.",
    "Latch the data input latch from the system bus.",
    "Latch the processor status register carry bit from the ALU carry output signal.",
    "Latch the data output register from the data bus.",
    "Latch the processor status register from the data bus.",
    "Latch the accumulator from the data bus.",
    "Latch the program counter low byte from the data bus.",
    "Latch the program counter high byte from the data bus.",
    "Latch the address bus register high byte from the data bus.",
    "Latch the ALU input B register from the data bus.",
    "Assert the accumulator onto the internal bus.",
    "Assert the index register onto the internal bus.",
    "Assert the program counter low byte onto the internal bus.",
    "Assert the program counter high byte onto the internal bus.",
    "Assert the ALU output register onto the internal bus.",
    "Assert the input data latch onto the internal bus.",
    "Assert the accumulator onto the data bus.",
    "Assert the program counter low byte onto the data bus.",
    "Assert the program counter high byte onto the data bus.",
    "Assert the input data latch onto the data bus.",
    "Assert the processor status register onto the data bus.",
    "Set the program counter increment signal. Increments the program counter.",
    "Set the CPU write signal. Asserts the data output register onto the system bus.",
    "Set the ALU SUM signal. Activates the addition circuit.",
    "Set the ALU SHR signal. Activates the right-shift circuit.",
    "Set the ALU OR signal. Activates the bitwise-or circuit.",
    "Set the ALU XOR signal. Activates the bitwise-xor circuit.",
    "Set the ALU AND signal. Activates the bitwise-and circuit.",
    "Set the ALU carry signal. Sets the input carry bit.",
    "Set the ALU negate signal. Negates BI while set.",
    "Set the ALU reset A signal. Resets the input A register to zero.",
    "Set the ALU reset B signal. Resets the input B register to zero.",
};

// clang-format off
// Presentation order - show tiles in an order corresponding to CPU order of execution
static const std::vector<uint8_t> sort_order{
    TgmCpu::Wire::sPI,
    TgmCpu::Wire::sSUM,
    TgmCpu::Wire::sSHR,
    TgmCpu::Wire::sOR,
    TgmCpu::Wire::sXOR,
    TgmCpu::Wire::sAND,
    TgmCpu::Wire::sAZ,
    TgmCpu::Wire::sBZ,
    TgmCpu::Wire::sNEG,
    TgmCpu::Wire::sCI,
    TgmCpu::Wire::lxDL,
    TgmCpu::Wire::adDL,
    TgmCpu::Wire::adAC,
    TgmCpu::Wire::adPCL,
    TgmCpu::Wire::adPCH,
    TgmCpu::Wire::adP,
    TgmCpu::Wire::aiDL,
    TgmCpu::Wire::aiAC,
    TgmCpu::Wire::aiX,
    TgmCpu::Wire::aiPCL,
    TgmCpu::Wire::aiPCH,
    TgmCpu::Wire::aiADD,
    TgmCpu::Wire::laADD,
    TgmCpu::Wire::laC,
    TgmCpu::Wire::ldDOR,
    TgmCpu::Wire::ldAC,
    TgmCpu::Wire::ldPCL,
    TgmCpu::Wire::ldPCH,
    TgmCpu::Wire::ldP,
    TgmCpu::Wire::ldABH,
    TgmCpu::Wire::ldBI,
    TgmCpu::Wire::liAC,
    TgmCpu::Wire::liX,
    TgmCpu::Wire::liPCL,
    TgmCpu::Wire::liPCH,
    TgmCpu::Wire::liABL,
    TgmCpu::Wire::liABH,
    TgmCpu::Wire::liAI,
    TgmCpu::Wire::liBI,
    TgmCpu::Wire::sW,
};
// clang-format on

static const float VerifyingTime = 1.0f;
static const float ResultTime = 2.0f;
static const float ShortFade = 0.125f;

static float clamp(float t, float min, float max)
{
    return t < min ? min : (t > max ? max : t);
}


static float ease_in(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - t, 3.0f);
}


static float ease_out(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return 1.0f - std::pow(t, 3.0f);
}


bool PuzzleState::on_update(float delta)
{
    // _state: 0 - fading in, 1 - puzzling, 2 - fading out
    if (_state == 0 && _state_timer >= ShortFade)
    {
        _state = 1;
        _app->show_mouse(true);
    }
    else if (_state == 2 && _state_timer >= ShortFade)
    {
        return false;
    }

    if (_state == 1)
    {
        if (!update_simulation(delta))
        {
            _state = 2;
            _state_timer = 0.0f;
        }
    }
    else
    {
        _state_timer += delta;
    }

    render(delta);

    if (_state == 0)
    {
        _app->set_screen_fade(App::BgColor, ease_out(_state_timer / ShortFade));
    }
    else if (_state == 2)
    {
        _app->set_screen_fade(App::BgColor, ease_in(_state_timer / ShortFade));
    }

    return true;
}


bool PuzzleState::update_simulation(float delta)
{
    V2i mouse_pos{ _app->mouse_state().x, _app->mouse_state().y };
    Recti go_button_rect{ { PuzzleBoardLayout::GoButtonX, PuzzleBoardLayout::GoButtonY }, { GoButtonW, GoButtonH } };

    // Update state
    if (_complete > 0.0f)
    {
        _complete -= delta;

        if (_complete <= 0.0f)
        {
            return false;
        }
    }
    else if (_verifying > 0.0f)
    {
        _verifying -= delta;

        if (_verifying <= 0.0f)
        {
            TgmCpu::Instruction instruction{};

            for (int clock = 0; clock < 4; ++clock)
            {
                for (int wire = 0; wire < 18; ++wire)
                {
                    uint8_t w = (uint8_t)_solution[clock * 18 + wire];

                    if (w)
                    {
                        instruction.Tn[clock].push_back(_linedefs[w - 1].line);
                    }
                }
            }

            _success = Puzzle::verify(*_puzzle, instruction);
            _complete = ResultTime;
        }
    }
    else
    {
        V2i mouse_pos{ _app->mouse_state().x, _app->mouse_state().y };
        Recti go_button_rect{ { PuzzleBoardLayout::GoButtonX, PuzzleBoardLayout::GoButtonY }, { GoButtonW, GoButtonH } };

        if (_go_pressed)
        {
            if (!_app->mouse_state().buttons[gli::MouseButton::Left].down)
            {
                _go_pressed = false;

                if (contains(go_button_rect, mouse_pos))
                {
                    _verifying = VerifyingTime;
                }
            }
        }
        else
        {
            if (_app->mouse_state().buttons[gli::MouseButton::Right].pressed)
            {
                if (_dragging)
                {
                    _dragging = 0;
                    _app->show_mouse(true);
                }
                else
                {
                    remove_from_solution(mouse_pos.x, mouse_pos.y);
                }
            }
            else if (_app->mouse_state().buttons[gli::MouseButton::Left].pressed)
            {
                if (_dragging)
                {
                    if (add_to_solution(mouse_pos.x, mouse_pos.y, _dragging - 1))
                    {
                        _dragging = 0;
                        _app->show_mouse(true);
                    }
                }
                else
                {
                    _dragging = select_from_bag(mouse_pos.x, mouse_pos.y);

                    if (_dragging)
                    {
                        _app->show_mouse(false);
                    }
                    else
                    {
                        _go_pressed = contains(go_button_rect, mouse_pos);
                    }
                }
            }
        }
    }

    _ghost_row = -1;

    if (_dragging)
    {
        V2i ghost_pos = mouse_pos - V2i{ PuzzleBoardLayout::SolutionX, PuzzleBoardLayout::SolutionY };
        if (ghost_pos.x < PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionColumns && ghost_pos.y < PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionRows)
        {
            _ghost_row = ghost_pos.y / PuzzleBoardLayout::SolutionStep;
        }
    }

    return true;
}

void PuzzleState::render(float delta)
{
    V2i mouse_pos{ _app->mouse_state().x, _app->mouse_state().y };
    Recti go_button_rect{ { PuzzleBoardLayout::GoButtonX, PuzzleBoardLayout::GoButtonY }, { GoButtonW, GoButtonH } };

    // Render
    _app->clear_screen(gli::Pixel(0x1F1D2C));
    _app->draw_sprite(0, 0, &_sprites[Sprite::Board]);


    _app->draw_text_box(PuzzleBoardLayout::OpcodeTitleX, PuzzleBoardLayout::OpcodeTitleY, PuzzleBoardLayout::OpcodeTitleW,
                        PuzzleBoardLayout::OpcodeTitleH, _puzzle->title, PuzzleBoardLayout::TextColor, PuzzleBoardLayout::TextBgColor);
    _app->draw_text_box(PuzzleBoardLayout::OpcodeDescX, PuzzleBoardLayout::OpcodeDescY, PuzzleBoardLayout::OpcodeDescW,
                        PuzzleBoardLayout::OpcodeDescH, _puzzle->description, PuzzleBoardLayout::TextColor, PuzzleBoardLayout::TextBgColor);

    int idx = 0;
    for (size_t def_index : _bag_order)
    {
        int bx = idx % PuzzleBoardLayout::BagColumns;
        int by = idx / PuzzleBoardLayout::BagColumns;
        int x = PuzzleBoardLayout::BagX + bx * PuzzleBoardLayout::BagStep;
        int y = PuzzleBoardLayout::BagY + by * PuzzleBoardLayout::BagStep;
        draw_tile(_linedefs[def_index], x, y, false);
        idx++;
    }

#if 0
    idx = 0;
    for (size_t def_index : _solution)
    {
        if (def_index)
        {
            int sx = idx % PuzzleBoardLayout::SolutionColumns;
            int sy = idx / PuzzleBoardLayout::SolutionColumns;
            int x = PuzzleBoardLayout::SolutionX + sx * PuzzleBoardLayout::SolutionStep;
            int y = PuzzleBoardLayout::SolutionY + sy * PuzzleBoardLayout::SolutionStep;
            draw_tile(_linedefs[def_index - 1], x, y, false);
        }
        idx++;
    }

    if (_ghost_row != -1)
    {
        int x = PuzzleBoardLayout::SolutionX;
        int y = PuzzleBoardLayout::SolutionY + _ghost_row * PuzzleBoardLayout::SolutionStep;
        int ox = (PuzzleTileSheet::Ghost % TileSheetLayout::Columns) * TileSheetLayout::TileSize;
        int oy = (PuzzleTileSheet::Ghost / TileSheetLayout::Columns) * TileSheetLayout::TileSize;
        _app->blend_partial_sprite(x, y, _sprites[Sprite::PiecesSheet], ox, oy, TileSheetLayout::TileSize, TileSheetLayout::TileSize, 255);
    }
#else
    for (int i = 0; i < PuzzleBoardLayout::SolutionRows; ++i)
    {
        std::vector<size_t> sorted = sort_solution_row(i, true);

        idx = 0;

        for (size_t def_index : sorted)
        {
            if (def_index)
            {
                int x = PuzzleBoardLayout::SolutionX + idx * PuzzleBoardLayout::SolutionStep;
                int y = PuzzleBoardLayout::SolutionY + i * PuzzleBoardLayout::SolutionStep;
                draw_tile(_linedefs[def_index - 1], x, y, false);
            }
            idx++;
        }
    }
#endif

    if (_complete > 0.0f)
    {
        int sprite = _success ? Sprite::ValidationSuccess : Sprite::ValidationFailed;
        int x = (_app->screen_width() - _sprites[sprite].width()) / 2;
        int y = (_app->screen_height() - _sprites[sprite].height()) / 2;
        _app->blend_sprite(x, y, _sprites[sprite], 255);
    }
    else if (_verifying > 0.0f)
    {
        int x = (_app->screen_width() - _sprites[Sprite::VerifyingDialog].width()) / 2;
        int y = (_app->screen_height() - _sprites[Sprite::VerifyingDialog].height()) / 2;
        _app->blend_sprite(x, y, _sprites[Sprite::VerifyingDialog], 255);
    }
    else
    {
        if (_go_pressed)
        {
            if (contains(go_button_rect, mouse_pos))
            {
                _app->draw_sprite(go_button_rect.origin.x, go_button_rect.origin.y, &_sprites[Sprite::GoButtonPressed]);
            }
        }
        else
        {
            // Tool tip
            size_t tipindex = _dragging;

            if (!tipindex)
            {
                tipindex = select_from_bag(mouse_pos.x, mouse_pos.y);

                if (!tipindex)
                {
                    tipindex = select_from_solution(mouse_pos.x, mouse_pos.y);
                }
            }

            if (tipindex)
            {
                const std::string& tiptext = ToolTips[tipindex - 1];
                _app->draw_text_box(PuzzleBoardLayout::ToolTipX, PuzzleBoardLayout::ToolTipY, PuzzleBoardLayout::ToolTipW,
                                    PuzzleBoardLayout::ToolTipH, tiptext, PuzzleBoardLayout::TextColor, PuzzleBoardLayout::TextBgColor);
            }

            if (_dragging)
            {
                draw_tile(_linedefs[_dragging - 1], _app->mouse_state().x, _app->mouse_state().y, true);
            }
        }
    }
}

bool PuzzleState::on_init(App* app)
{
    _app = app;

    static const char* sprite_files[Sprite::Count] = { GliAssetPath("gui/puzzle_board.png"),    GliAssetPath("gui/puzzle_pieces_sheet.png"),
                                                       GliAssetPath("gui/go_btn_down.png"),     GliAssetPath("gui/verifying.png"),
                                                       GliAssetPath("gui/validation_fail.png"), GliAssetPath("gui/validation_success.png") };

    size_t idx = 0;

    for (gli::Sprite& sprite : _sprites)
    {
        if (!sprite.load(sprite_files[idx]))
        {
            return false;
        }
        idx++;
    }

    // clang-format off
    std::vector<ControlLineDef> defs
    {
        { TgmCpu::Wire::liAC,   PuzzleTileSheet::lblAC,    PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liX,    PuzzleTileSheet::lblX,     PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liPCL,  PuzzleTileSheet::lblPCL,   PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liPCH,  PuzzleTileSheet::lblPCH,   PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liABL,  PuzzleTileSheet::lblABL,   PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liABH,  PuzzleTileSheet::lblABH,   PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liAI,   PuzzleTileSheet::lblAI,    PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::liBI,   PuzzleTileSheet::lblBI,    PuzzleTileSheet::bLatch_i      },
        { TgmCpu::Wire::laADD,  PuzzleTileSheet::lblADD,   PuzzleTileSheet::bLatch_x      },
        { TgmCpu::Wire::lxDL,   PuzzleTileSheet::lblDL,    PuzzleTileSheet::bLatch_x      },
        { TgmCpu::Wire::laC,    PuzzleTileSheet::lblC,     PuzzleTileSheet::bLatch_x      },
        { TgmCpu::Wire::ldDOR,  PuzzleTileSheet::lblDOR,   PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::ldP,    PuzzleTileSheet::lblP,     PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::ldAC,   PuzzleTileSheet::lblAC,    PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::ldPCL,  PuzzleTileSheet::lblPCL,   PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::ldPCH,  PuzzleTileSheet::lblPCH,   PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::ldABH,  PuzzleTileSheet::lblABH,   PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::ldBI,   PuzzleTileSheet::lblBI,    PuzzleTileSheet::bLatch_d      },
        { TgmCpu::Wire::aiAC,   PuzzleTileSheet::lblAC,    PuzzleTileSheet::bAssert_i     },
        { TgmCpu::Wire::aiX,    PuzzleTileSheet::lblX,     PuzzleTileSheet::bAssert_i     },
        { TgmCpu::Wire::aiPCL,  PuzzleTileSheet::lblPCL,   PuzzleTileSheet::bAssert_i     },
        { TgmCpu::Wire::aiPCH,  PuzzleTileSheet::lblPCH,   PuzzleTileSheet::bAssert_i     },
        { TgmCpu::Wire::aiADD,  PuzzleTileSheet::lblADD,   PuzzleTileSheet::bAssert_i     },
        { TgmCpu::Wire::aiDL,   PuzzleTileSheet::lblDL,    PuzzleTileSheet::bAssert_i     },
        { TgmCpu::Wire::adAC,   PuzzleTileSheet::lblAC,    PuzzleTileSheet::bAssert_d     },
        { TgmCpu::Wire::adPCL,  PuzzleTileSheet::lblPCL,   PuzzleTileSheet::bAssert_d     },
        { TgmCpu::Wire::adPCH,  PuzzleTileSheet::lblPCH,   PuzzleTileSheet::bAssert_d     },
        { TgmCpu::Wire::adDL,   PuzzleTileSheet::lblDL,    PuzzleTileSheet::bAssert_d     },
        { TgmCpu::Wire::adP,    PuzzleTileSheet::lblP,     PuzzleTileSheet::bAssert_d     },
        { TgmCpu::Wire::sPI,    PuzzleTileSheet::lblPI,    PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sW,     PuzzleTileSheet::lblW,     PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sSUM,   PuzzleTileSheet::lblSUM,   PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sSHR,   PuzzleTileSheet::lblSHR,   PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sOR,    PuzzleTileSheet::lblOR,    PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sXOR,   PuzzleTileSheet::lblXOR,   PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sAND,   PuzzleTileSheet::lblAND,   PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sCI,    PuzzleTileSheet::lblCI,    PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sNEG,   PuzzleTileSheet::lblNEG,   PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sAZ,    PuzzleTileSheet::lblAZ,    PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::sBZ,    PuzzleTileSheet::lblBZ,    PuzzleTileSheet::bSet          },
        { TgmCpu::Wire::Count,  PuzzleTileSheet::Ghost,    PuzzleTileSheet::Ghost         },
    };
    // clang-format on

    _linedefs.insert(_linedefs.end(), defs.begin(), defs.end());
    _bag_order = sort_bag();

    return true;
}


void PuzzleState::on_destroy() {}


bool PuzzleState::on_enter()
{
    if (!_puzzle)
    {
        return false;
    }

    _go_pressed = false;
    _dragging = 0;
    _solution.clear();
    _solution.resize(PuzzleBoardLayout::SolutionRows * PuzzleBoardLayout::SolutionColumns);
    _verifying = 0.0f;
    _complete = 0.0f;
    _success = false;
    _state_timer = 0.0f;
    _state = 0;
    _ghost_row = -1;
    return true;
}


void PuzzleState::on_exit()
{
    _app->show_mouse(false);
}


void PuzzleState::on_suspend() {}

void PuzzleState::on_resume() {}


void PuzzleState::draw_tile(const ControlLineDef& linedef, int x, int y, bool center)
{
    int bgtile = linedef.bgtile;
    int fgtile = linedef.label;

    if (center)
    {
        x -= TileSheetLayout::TileSize / 2;
        y -= TileSheetLayout::TileSize / 2;
    }

    int ox;
    int oy;

    if (bgtile != PuzzleTileSheet::Ghost)
    {
        ox = (bgtile % TileSheetLayout::Columns) * TileSheetLayout::TileSize;
        oy = (bgtile / TileSheetLayout::Columns) * TileSheetLayout::TileSize;
        _app->draw_partial_sprite(x, y, &_sprites[Sprite::PiecesSheet], ox, oy, TileSheetLayout::TileSize, TileSheetLayout::TileSize);
    }

    ox = (fgtile % TileSheetLayout::Columns) * TileSheetLayout::TileSize;
    oy = (fgtile / TileSheetLayout::Columns) * TileSheetLayout::TileSize;
    _app->blend_partial_sprite(x, y, _sprites[Sprite::PiecesSheet], ox, oy, TileSheetLayout::TileSize, TileSheetLayout::TileSize, 255);
}


size_t PuzzleState::select_from_bag(int x, int y)
{
    V2i pos{ x, y };
    Recti bag_rect{ { PuzzleBoardLayout::BagX, PuzzleBoardLayout::BagY },
                    { PuzzleBoardLayout::BagStep * PuzzleBoardLayout::BagColumns - 1, PuzzleBoardLayout::BagStep * PuzzleBoardLayout::BagRows - 1 } };
    size_t selected = 0;

    if (contains(bag_rect, pos))
    {
        V2i offset = pos - bag_rect.origin;
        selected = (offset.x / PuzzleBoardLayout::BagStep) + (offset.y / PuzzleBoardLayout::BagStep) * PuzzleBoardLayout::BagColumns;

        if (selected >= _bag_order.size())
        {
            selected = 0;
        }
        else
        {
            selected = _bag_order[selected] + 1;
        }
    }

    return selected;
}


size_t PuzzleState::select_from_solution(int x, int y)
{
    V2i pos{ x, y };
    Recti solution_rect{ { PuzzleBoardLayout::SolutionX, PuzzleBoardLayout::SolutionY },
                         { PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionColumns,
                           PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionRows } };
    size_t tile = 0;

    if (contains(solution_rect, pos))
    {
        V2i offset = pos - solution_rect.origin;
        int row = offset.y / PuzzleBoardLayout::SolutionStep;
        int column = offset.x / PuzzleBoardLayout::SolutionStep;

        if (row < PuzzleBoardLayout::SolutionRows && column < PuzzleBoardLayout::SolutionColumns)
        {
            std::vector<size_t> sorted = sort_solution_row(row, false);
            size_t index = row * PuzzleBoardLayout::SolutionColumns + column;
            tile = _solution[index];
        }
    }

    return tile;
}


void PuzzleState::remove_from_solution(int x, int y)
{
    V2i pos{ x, y };
    Recti solution_rect{ { PuzzleBoardLayout::SolutionX, PuzzleBoardLayout::SolutionY },
                         { PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionColumns,
                           PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionRows } };

    if (contains(solution_rect, pos))
    {
        V2i offset = pos - solution_rect.origin;
#if 0
        size_t selected =
                (offset.x / PuzzleBoardLayout::SolutionStep) + (offset.y / PuzzleBoardLayout::SolutionStep) * PuzzleBoardLayout::SolutionColumns;

        if (selected < SolutionRows * SolutionColumns)
        {
            _solution.control_lines[selected] = 0;
        }
#else
        int row_base = (offset.y / PuzzleBoardLayout::SolutionStep) * PuzzleBoardLayout::SolutionColumns;

        if (row_base < PuzzleBoardLayout::SolutionRows * PuzzleBoardLayout::SolutionColumns)
        {
            for (int col = (offset.x / PuzzleBoardLayout::SolutionStep); col < PuzzleBoardLayout::SolutionColumns - 1; ++col)
            {
                _solution[col + row_base] = _solution[col + 1 + row_base];
            }

            _solution[PuzzleBoardLayout::SolutionColumns - 1 + row_base] = 0;
        }
#endif
    }
}

bool PuzzleState::add_to_solution(int x, int y, size_t def)
{
    V2i pos{ x, y };
    Recti solution_rect{ { PuzzleBoardLayout::SolutionX, PuzzleBoardLayout::SolutionY },
                         { PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionColumns,
                           PuzzleBoardLayout::SolutionStep * PuzzleBoardLayout::SolutionRows } };

    if (contains(solution_rect, pos))
    {
        V2i offset = pos - solution_rect.origin;
#if 0
            size_t selected = (offset.x / PuzzleBoardLayout::SolutionStep) + (offset.y / PuzzleBoardLayout::SolutionStep) * PuzzleBoardLayout::SolutionColumns;

            if (selected < SolutionRows * SolutionColumns)
            {
                _solution.control_lines[selected] = def + 1;
                return true;
            }
#else
        int row_base = (offset.y / PuzzleBoardLayout::SolutionStep) * PuzzleBoardLayout::SolutionColumns;

        if (row_base < PuzzleBoardLayout::SolutionRows * PuzzleBoardLayout::SolutionColumns)
        {
            for (int col = 0; col < PuzzleBoardLayout::SolutionColumns; ++col)
            {
                if (_solution[col + row_base] == 0)
                {
                    _solution[col + row_base] = def + 1;
                    return true;
                }
            }
        }

#endif
    }

    return false;
}


std::vector<size_t> PuzzleState::sort_solution_row(int row, bool add_ghost)
{
    // Sort the solution row so that signals and control bus wires appear in the order they take effect
    // DL -> Signals (other than write) -> ASSERTS -> LATCHES (other than DL) -> W
    // Include the ghost (if it's in this row and can be added)
    std::vector<size_t> result;
    result.reserve(PuzzleBoardLayout::SolutionColumns);

    for (size_t i = 0; i < PuzzleBoardLayout::SolutionColumns; ++i)
    {
        if (_solution[i + row * PuzzleBoardLayout::SolutionColumns])
        {
            result.push_back(_solution[i + row * PuzzleBoardLayout::SolutionColumns]);
        }
        else
        {
            break;
        }
    }

    if (add_ghost && row == _ghost_row && result.size() < PuzzleBoardLayout::SolutionColumns)
    {
        result.push_back(_linedefs.size());
    }

#if 0
    std::sort(result.begin(), result.end(), [this](const size_t& a, const size_t& b) -> bool
    {
        const ControlLineDef& la = (a == _linedefs.size()) ? _linedefs[_dragging - 1] : _linedefs[a - 1];
        const ControlLineDef& lb = (b == _linedefs.size()) ? _linedefs[_dragging - 1] : _linedefs[b - 1];
        auto ita = std::find(sort_order.begin(), sort_order.end(), la.line);
        auto itb = std::find(sort_order.begin(), sort_order.end(), lb.line);
        bool result = true;

        if (ita != sort_order.end() && itb != sort_order.end())
        {
            result = std::distance(sort_order.begin(), ita) < std::distance(sort_order.begin(), itb);
        }

        return result;
    });
#endif

    return result;
}

std::vector<size_t> PuzzleState::sort_bag()
{
    std::vector<size_t> result(_linedefs.size() - 1);
    std::iota(result.begin(), result.end(), 0);
    std::sort(result.begin(), result.end(), [this](const size_t& a, const size_t& b) -> bool {
        const ControlLineDef& la = _linedefs[a];
        const ControlLineDef& lb = _linedefs[b];
        auto ita = std::find(sort_order.begin(), sort_order.end(), la.line);
        auto itb = std::find(sort_order.begin(), sort_order.end(), lb.line);
        bool result = true;

        if (ita != sort_order.end() && itb != sort_order.end())
        {
            result = std::distance(sort_order.begin(), ita) < std::distance(sort_order.begin(), itb);
        }

        return result;
    });

    return result;
}

} // namespace Bootstrap