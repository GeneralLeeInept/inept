#pragma once

#include "types.h"

#include <vector>

namespace Bootstrap
{

// The Great Machine CPU emulator core.

class TgmCpu
{
public:
    enum Wire
    {
        first_latch,
        _latches_begin = first_latch - 1,
        liAC,  // "Latch the accumulator from the internal bus.",
        liX,   // "Latch the index register from the internal bus.",
        liPCL, // "Latch the program counter low byte from the internal bus.",
        liPCH, // "Latch the program counter high byte from the internal bus.",
        liABL, // "Latch the address bus register low byte from the internal bus.",
        liABH, // "Latch the address bus register high byte from the internal bus.",
        liAI,  // "Latch the ALU input A register from the internal bus.",
        liBI,  // "Latch the ALU input B register from the internal bus.",
        laADD, // "Latch the ALU output register from the current output of the ALU.",
        lxDL,  // "Latch the data input latch from the system bus.",
        laC,   // "Latch the processor status register carry bit from the ALU carry output signal.",
        ldDOR, // "Latch the data output register from the data bus.",
        ldP,   // "Latch the processor status register from the data bus.",
        ldAC,  // "Latch the accumulator from the data bus.",
        ldPCL, // "Latch the program counter low byte from the data bus.",
        ldPCH, // "Latch the program counter high byte from the data bus.",
        ldABH, // "Latch the address bus register high byte from the data bus.",
        ldBI,  // "Latch the ALU input B register from the data bus.",
        _latches_end,
        last_latch = _latches_end - 1,

        first_assert,
        _asserts_begin = first_assert - 1,
        aiAC,  // "Assert the accumulator onto the internal bus.",
        aiX,   // "Assert the index register onto the internal bus.",
        aiPCL, // "Assert the program counter low byte onto the internal bus.",
        aiPCH, // "Assert the program counter high byte onto the internal bus.",
        aiADD, // "Assert the ALU output register onto the internal bus.",
        aiDL,  // "Assert the input data latch onto the internal bus.",
        adAC,  // "Assert the accumulator onto the data bus.",
        adPCL, // "Assert the program counter low byte onto the data bus.",
        adPCH, // "Assert the program counter high byte onto the data bus.",
        adDL,  // "Assert the input data latch onto the data bus.",
        adP,   // "Assert the processor status register onto the data bus.",
        _asserts_end,
        last_assert = _asserts_end - 1,

        first_signal,
        _signals_begin = first_signal - 1,
        sPI,   // "Set the program counter increment signal. Increments the program counter.",
        sW,    // "Set the CPU write signal. Asserts the data output register onto the system bus.",
        sSUM,  // "Set the ALU SUM signal. Activates the addition circuit.",
        sSHR,  // "Set the ALU SHR signal. Activates the right-shift circuit.",
        sOR,   // "Set the ALU OR signal. Activates the bitwise-or circuit.",
        sXOR,  // "Set the ALU XOR signal. Activates the bitwise-xor circuit.",
        sAND,  // "Set the ALU AND signal. Activates the bitwise-and circuit.",
        sCI,   // "Set the ALU carry signal. Sets the input carry bit.",
        sNEG,  // "Set the ALU negate signal. Negates BI while set.",
        sAZ,   // "Set the ALU reset A signal. Resets the input A register to zero.",
        sBZ,   // "Set the ALU reset B signal. Resets the input B register to zero.",
        _signals_end,
        last_signal = _signals_end - 1,

        Count
    };

    union Reg16
    {
        uint16_t value;
        struct
        {
            uint8_t lo;
            uint8_t hi;
        };
    };

    struct ALU
    {
        uint8_t input_a;
        uint8_t input_b;
        uint8_t output_bus;
        uint8_t output;
        uint8_t carry;
        uint8_t sum_result;
        uint8_t shr_result;
        uint8_t or_result;
        uint8_t xor_result;
        uint8_t and_result;

        void calculate(bool wires[Wire::Count]);
    };

    union StatusRegister
    {
        uint8_t value;
        struct
        {
            uint8_t carry : 1;
            uint8_t unused : 7;
        };
    };

    struct Instruction
    {
        std::vector<uint8_t> Tn[4];
    };

    // CPU execution
    void execute(const Instruction& instruction);
    void clock(const std::vector<uint8_t>& lines);

    // Helpers
    static bool isLatch(uint8_t w) { return w >= Wire::first_latch && w <= Wire::last_latch; }
    static bool isAssert(uint8_t w) { return w >= Wire::first_assert && w <= Wire::last_assert; }
    static bool isSignal(uint8_t w) { return w >= Wire::first_signal && w <= Wire::last_signal; }

    // Buses
    uint8_t system_bus;
    uint8_t internal_bus;
    uint8_t data_bus;
    uint16_t address_bus;

    // RAM - 64K addressable. We don't need no ROM or I/O.
    uint8_t ram[64 * 1024];

    // Circuits
    Reg16 program_counter;
    Reg16 address_bus_register;
    StatusRegister status_register;
    uint8_t accumulator;
    uint8_t index_register;
    uint8_t data_output_register;
    uint8_t input_data_latch;
    ALU alu;

    // Control lines and signals
    bool wires[Wire::Count];

    // Emulation state
    uint8_t clock_phase; // 0: first rising edge, 1: first falling edge, 2: second rising edge, 3: second falling edge
    uint8_t instruction_cycle;
};
} // namespace Bootstrap