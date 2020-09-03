#include "puzzlestate.h"

#include "assets.h"
#include "bootstrap.h"
#include "collision.h"
#include "random.h"
#include "types.h"

namespace Bootstrap
{

enum ControlLineMnemonic
{
    ABH,
    ABL,
    AC,
    AND,
    ADD,
    AI,
    AZ,
    BI,
    BZ,
    C,
    CI,
    DL,
    DOR,
    NEG,
    OR,
    P,
    PCH,
    PCL,
    PI,
    SHR,
    SUM,
    W,
    X,
    XOR,
};

enum ControlLineBehavior
{
    Assert,
    Latch,
    Set
};

enum ControlLineTarget
{
    DataBus,
    InternalBus,
    ALU,
    WSignal,
    PCISignal,
    StatusRegister,
    SystemBus,
};

enum PuzzleTileSheet
{
    bAssert_d,
    bAssert_i,
    bLatch_d,
    bLatch_i,
    bSet,
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
    lblC
};

struct ControlLineDef
{
    int line;
    int bgtile;
    int fgtile;
};

enum TileSheetLayout
{
    TileSize = 30,
    Columns = 10
};

enum PuzzleBoardLayout
{
    SolutionOX = 49,
    SolutionOY = 17,
    SolutionColumns = 18,
    SolutionRows = 4,
    SolutionStep = 32,
    BagOX = 208,
    BagOY = 147,
    BagStep = 32,
    BagColumns = 7,
    BagRows = 6,
    OpcodeDescX = 32,
    OpcodeDescY = 163,
    OpcodeDescW = 158,
    OpcodeDescH = 114,
    GoButtonX = 31,
    GoButtonY = 283,
    GoButtonW = 160,
    GoButtonH = 32,
    ToolTipX = 450,
    ToolTipY = 157,
    ToolTipW = 158,
    ToolTipH = 167
};


static const std::string ControlLineDescriptions[] = {
    "Latch the accumulator from the internal bus.",
    "Latch the index  register from the internal bus.",
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
    "Assert the input data latch onto the internal bus.",
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

bool PuzzleState::on_update(float delta)
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
            _complete = 2.0f;
            _success = (gRandom.get() > 0.5f);
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
                    _verifying = 2.0f;
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

    // Render
    _app->clear_screen(gli::Pixel(0x1F1D2C));
    _app->draw_sprite(0, 0, &_sprites[Sprite::Board]);

    int idx = 0;
    for (const ControlLineDef& linedef : _linedefs)
    {
        int bx = idx % PuzzleBoardLayout::BagColumns;
        int by = idx / PuzzleBoardLayout::BagColumns;
        int x = PuzzleBoardLayout::BagOX + bx * PuzzleBoardLayout::BagStep;
        int y = PuzzleBoardLayout::BagOY + by * PuzzleBoardLayout::BagStep;
        draw_tile(linedef, x, y, false);
        idx++;
    }

    idx = 0;
    for (const size_t& def_index : _solution)
    {
        if (def_index)
        {
            int sx = idx % PuzzleBoardLayout::SolutionColumns;
            int sy = idx / PuzzleBoardLayout::SolutionColumns;
            int x = PuzzleBoardLayout::SolutionOX + sx * PuzzleBoardLayout::SolutionStep;
            int y = PuzzleBoardLayout::SolutionOY + sy * PuzzleBoardLayout::SolutionStep;
            draw_tile(_linedefs[def_index - 1], x, y, false);
        }
        idx++;
    }

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

        if (_dragging)
        {
            draw_tile(_linedefs[_dragging - 1], _app->mouse_state().x, _app->mouse_state().y, true);
        }
    }

    return true;
}

bool PuzzleState::on_init(App* app)
{
    _app = app;

    static const char* sprite_files[Sprite::Count] = {
        GliAssetPath("gui/puzzle_board.png"),
        GliAssetPath("gui/puzzle_pieces_sheet.png"),
        GliAssetPath("gui/go_btn_down.png"),
        GliAssetPath("gui/verifying.png"),
        GliAssetPath("gui/validation_fail.png"),
        GliAssetPath("gui/validation_success.png")
    };

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
    std::vector<ControlLineDef> defs{
        { ControlLineMnemonic::AC,    ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::X,     ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::PCL,   ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::PCH,   ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::ABL,   ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::ABH,   ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::AI,    ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::BI,    ControlLineBehavior::Latch,     ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::ADD,   ControlLineBehavior::Latch,     ControlLineTarget::ALU              },
        { ControlLineMnemonic::DL,    ControlLineBehavior::Latch,     ControlLineTarget::SystemBus        },
        { ControlLineMnemonic::C,     ControlLineBehavior::Latch,     ControlLineTarget::StatusRegister   },
        { ControlLineMnemonic::DOR,   ControlLineBehavior::Latch,     ControlLineTarget::DataBus          },
        { ControlLineMnemonic::P,     ControlLineBehavior::Latch,     ControlLineTarget::DataBus          },
        { ControlLineMnemonic::AC,    ControlLineBehavior::Latch,     ControlLineTarget::DataBus          },
        { ControlLineMnemonic::PCL,   ControlLineBehavior::Latch,     ControlLineTarget::DataBus          },
        { ControlLineMnemonic::PCH,   ControlLineBehavior::Latch,     ControlLineTarget::DataBus          },
        { ControlLineMnemonic::BI,    ControlLineBehavior::Latch,     ControlLineTarget::DataBus          },
        { ControlLineMnemonic::AC,    ControlLineBehavior::Assert,    ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::X,     ControlLineBehavior::Assert,    ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::PCL,   ControlLineBehavior::Assert,    ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::PCH,   ControlLineBehavior::Assert,    ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::ADD,   ControlLineBehavior::Assert,    ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::DL,    ControlLineBehavior::Assert,    ControlLineTarget::InternalBus      },
        { ControlLineMnemonic::AC,    ControlLineBehavior::Assert,    ControlLineTarget::DataBus          },
        { ControlLineMnemonic::PCL,   ControlLineBehavior::Assert,    ControlLineTarget::DataBus          },
        { ControlLineMnemonic::PCH,   ControlLineBehavior::Assert,    ControlLineTarget::DataBus          },
        { ControlLineMnemonic::DL,    ControlLineBehavior::Assert,    ControlLineTarget::DataBus          },
        { ControlLineMnemonic::P,     ControlLineBehavior::Assert,    ControlLineTarget::DataBus          },
        { ControlLineMnemonic::PI,    ControlLineBehavior::Set,       ControlLineTarget::PCISignal        },
        { ControlLineMnemonic::W,     ControlLineBehavior::Set,       ControlLineTarget::WSignal          },
        { ControlLineMnemonic::SUM,   ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::SHR,   ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::OR,    ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::XOR,   ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::AND,   ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::CI,    ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::NEG,   ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::AZ,    ControlLineBehavior::Set,       ControlLineTarget::ALU              },
        { ControlLineMnemonic::BZ,    ControlLineBehavior::Set,       ControlLineTarget::ALU              },
    };
    // clang-format on

    _linedefs.insert(_linedefs.end(), defs.begin(), defs.end());

    return true;
}


void PuzzleState::on_destroy() {}


bool PuzzleState::on_enter()
{
    _app->show_mouse(true);
    _go_pressed = false;
    _dragging = 0;
    _solution.clear();
    _solution.resize(PuzzleBoardLayout::SolutionRows * PuzzleBoardLayout::SolutionColumns);
    _verifying = 0.0f;
    _complete = 0.0f;
    _success = false;
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
    int bgtile = PuzzleTileSheet::bAssert_i;
    int fgtile = PuzzleTileSheet::bAssert_i;

    if (linedef.behavior == ControlLineBehavior::Assert)
    {
        // clang-format off
        switch (linedef.target)
        {
            case ControlLineTarget::InternalBus:    bgtile = PuzzleTileSheet::bAssert_i; break;
            case ControlLineTarget::DataBus:        bgtile = PuzzleTileSheet::bAssert_d; break;
            default:                                gliAssert(!"Unhandled behavior/target combo"); break;
        }
        // clang-format on
    }
    else if (linedef.behavior == ControlLineBehavior::Latch)
    {
        // clang-format off
        switch (linedef.target)
        {
            case ControlLineTarget::DataBus:        bgtile = PuzzleTileSheet::bLatch_d; break;
            case ControlLineTarget::InternalBus:    bgtile = PuzzleTileSheet::bLatch_i; break;
            case ControlLineTarget::ALU:            bgtile = PuzzleTileSheet::bLatch_i; break;
            case ControlLineTarget::StatusRegister: bgtile = PuzzleTileSheet::bLatch_i; break;
            case ControlLineTarget::SystemBus:      bgtile = PuzzleTileSheet::bLatch_d; break;
            default:                                gliAssert(!"Unhandled behavior/target combo"); break;
        }
        // clang-format on
    }
    else if (linedef.behavior == ControlLineBehavior::Set)
    {
        bgtile = PuzzleTileSheet::bSet;
    }
    else
    {
        gliAssert(!"Unrecognized behavior");
    }

    // clang-format off
    switch (linedef.line)
    {
        case ControlLineMnemonic::ABH:  fgtile = PuzzleTileSheet::lblABH;   break;
        case ControlLineMnemonic::ABL:  fgtile = PuzzleTileSheet::lblABL;   break;
        case ControlLineMnemonic::AC:   fgtile = PuzzleTileSheet::lblAC;    break;
        case ControlLineMnemonic::AND:  fgtile = PuzzleTileSheet::lblAND;   break;
        case ControlLineMnemonic::ADD:  fgtile = PuzzleTileSheet::lblADD;   break;
        case ControlLineMnemonic::AI:   fgtile = PuzzleTileSheet::lblAI;    break;
        case ControlLineMnemonic::AZ:   fgtile = PuzzleTileSheet::lblAZ;    break;
        case ControlLineMnemonic::BI:   fgtile = PuzzleTileSheet::lblBI;    break;
        case ControlLineMnemonic::BZ:   fgtile = PuzzleTileSheet::lblBZ;    break;
        case ControlLineMnemonic::C:    fgtile = PuzzleTileSheet::lblC;     break;
        case ControlLineMnemonic::CI:   fgtile = PuzzleTileSheet::lblCI;    break;
        case ControlLineMnemonic::DL:   fgtile = PuzzleTileSheet::lblDL;    break;
        case ControlLineMnemonic::DOR:  fgtile = PuzzleTileSheet::lblDOR;   break;
        case ControlLineMnemonic::NEG:  fgtile = PuzzleTileSheet::lblNEG;   break;
        case ControlLineMnemonic::OR:   fgtile = PuzzleTileSheet::lblOR;    break;
        case ControlLineMnemonic::P:    fgtile = PuzzleTileSheet::lblP;     break;
        case ControlLineMnemonic::PCH:  fgtile = PuzzleTileSheet::lblPCH;   break;
        case ControlLineMnemonic::PCL:  fgtile = PuzzleTileSheet::lblPCL;   break;
        case ControlLineMnemonic::PI:   fgtile = PuzzleTileSheet::lblPI;    break;
        case ControlLineMnemonic::SHR:  fgtile = PuzzleTileSheet::lblSHR;   break;
        case ControlLineMnemonic::SUM:  fgtile = PuzzleTileSheet::lblSUM;   break;
        case ControlLineMnemonic::W:    fgtile = PuzzleTileSheet::lblW;     break;
        case ControlLineMnemonic::X:    fgtile = PuzzleTileSheet::lblX;     break;
        case ControlLineMnemonic::XOR:  fgtile = PuzzleTileSheet::lblXOR;   break;
        default:                        gliAssert(!"Unrecognized line");    break;
    }
    // clang-format on

    if (center)
    {
        x -= TileSheetLayout::TileSize / 2;
        y -= TileSheetLayout::TileSize / 2;
    }

    int ox = (bgtile % TileSheetLayout::Columns) * TileSheetLayout::TileSize;
    int oy = (bgtile / TileSheetLayout::Columns) * TileSheetLayout::TileSize;
    _app->draw_partial_sprite(x, y, &_sprites[Sprite::PiecesSheet], ox, oy, TileSheetLayout::TileSize, TileSheetLayout::TileSize);

    ox = (fgtile % TileSheetLayout::Columns) * TileSheetLayout::TileSize;
    oy = (fgtile / TileSheetLayout::Columns) * TileSheetLayout::TileSize;
    _app->blend_partial_sprite(x, y, _sprites[Sprite::PiecesSheet], ox, oy, TileSheetLayout::TileSize, TileSheetLayout::TileSize, 255);
}


size_t PuzzleState::select_from_bag(int x, int y)
{
    V2i pos{ x, y };
    Recti bag_rect{ { PuzzleBoardLayout::BagOX, PuzzleBoardLayout::BagOY },
                    { PuzzleBoardLayout::BagStep * PuzzleBoardLayout::BagColumns, PuzzleBoardLayout::BagStep * PuzzleBoardLayout::BagRows } };
    size_t selected = 0;

    if (contains(bag_rect, pos))
    {
        V2i offset = pos - bag_rect.origin;
        selected = (offset.x / PuzzleBoardLayout::BagStep) + (offset.y / PuzzleBoardLayout::BagStep) * PuzzleBoardLayout::BagColumns;

        if (selected >= _linedefs.size())
        {
            selected = 0;
        }
        else
        {
            selected = selected + 1;
        }
    }

    return selected;
}


void PuzzleState::remove_from_solution(int x, int y)
{
    V2i pos{ x, y };
    Recti solution_rect{ { PuzzleBoardLayout::SolutionOX, PuzzleBoardLayout::SolutionOY },
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
    Recti solution_rect{ { PuzzleBoardLayout::SolutionOX, PuzzleBoardLayout::SolutionOY },
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

} // namespace Bootstrap
