#include "tgmcpu.h"

namespace Bootstrap
{

void TgmCpu::ALU::calculate(bool wires[Wire::Count])
{
    uint16_t a = wires[Wire::sAZ] ? 0 : input_a;
    uint16_t b = wires[Wire::sBZ] ? 0 : input_b;
    uint16_t c = wires[Wire::sCI] ? 1 : 0;

    if (wires[Wire::sNEG])
    {
        b = (uint16_t)(-(unsigned int)b);
    }

    output_bus = 0;
    carry = 0;

    if (wires[Wire::sSUM])
    {
        uint16_t temp = a + b + c;
        output_bus |= (uint8_t)(temp);
        carry |= ((temp >> 8) & 1);
    }

    if (wires[Wire::sSHR])
    {
        output_bus |= (b >> 1);
        carry |= (b & 1);
    }

    if (wires[Wire::sOR])
    {
        output_bus |= (a | b);
    }

    if (wires[Wire::sXOR])
    {
        output_bus |= (a ^ b);
    }

    if (wires[Wire::sAND])
    {
        output_bus |= (a & b);
    }
}


// Before calling this function setup the following CPU state as required:
// - contents of RAM
// - program_counter (should be pointing to the byte after the opcode being executed)
// - status_register
// - accumulator
// - index_register
void TgmCpu::execute(const Instruction& instruction)
{
    // Prologue - uninitialize invisible state, set PC to address bus register
    alu.input_a = 0xdead;
    alu.input_b = 0xbeef;
    input_data_latch = 0xa5;
    data_output_register = 0x5a;
    address_bus_register.value = program_counter.value;
    for (int c = 0; c < 4; ++c)
    {
        clock(instruction.Tn[c]);
    }
}


void TgmCpu::clock(const std::vector<uint8_t>& lines)
{
    memset(wires, 0, sizeof(wires));

    system_bus = ram[address_bus_register.value];

    // 1. On the first rising edge the input data latch latches the system bus (if lDL pulled down) and signals are set. The program counter increments if the PI signal is set.
    for (uint8_t w : lines)
    {
        if (w == Wire::lxDL)
        {
            input_data_latch = system_bus;
        }
        else if (isSignal(w))
        {
            wires[w] = true;
        }
    }

    if (wires[Wire::sPI])
    {
        program_counter.value++;
    }

    // 2. On the first falling edge asserts are pulled down (buses are written).
    internal_bus = 0;
    data_bus = 0;

    for (uint8_t w : lines)
    {
        // clang-format off
        switch (w)
        {
            case Wire::aiAC:  internal_bus |= accumulator; break;
            case Wire::aiX:   internal_bus |= index_register; break;
            case Wire::aiPCL: internal_bus |= program_counter.lo; break;
            case Wire::aiPCH: internal_bus |= program_counter.hi; break;
            case Wire::aiADD: internal_bus |= alu.output; break;
            case Wire::aiDL:  internal_bus |= input_data_latch; break;
            case Wire::adAC:  data_bus |= accumulator; break;
            case Wire::adPCL: data_bus |= program_counter.lo; break;
            case Wire::adPCH: data_bus |= program_counter.hi; break;
            case Wire::adDL:  data_bus |= input_data_latch; break;
            case Wire::adP:   data_bus |= status_register.value; break;
        }
        // clang-format on
    }

    // 3. On the second rising edge circuits update their buffers and latches.
    alu.calculate(wires);

    // 4. On the second falling edge latch control lines other than lDL are pulled low (buses are read).
    for (uint8_t w : lines)
    {
        // clang-format off
        switch (w)
        {
            case Wire::liAC:    accumulator = internal_bus; break;
            case Wire::liX:     index_register = internal_bus; break;
            case Wire::liPCL:   program_counter.lo = internal_bus; break;
            case Wire::liPCH:   program_counter.hi = internal_bus; break;
            case Wire::liABL:   address_bus_register.lo = internal_bus; break;
            case Wire::liABH:   address_bus_register.hi = internal_bus; break;
            case Wire::liAI:    alu.input_a = internal_bus; break;
            case Wire::liBI:    alu.input_b = internal_bus; break;
            case Wire::laADD:   alu.output = alu.output_bus; break;
            case Wire::laC:     status_register.carry = alu.carry; break;
            case Wire::ldDOR:   data_output_register = data_bus; break;
            case Wire::ldP:     status_register.value = data_bus; break;
            case Wire::ldAC:    accumulator = data_bus; break;
            case Wire::ldPCL:   program_counter.lo = data_bus; break;
            case Wire::ldPCH:   program_counter.hi = data_bus; break;
            case Wire::ldABH:   address_bus_register.hi = data_bus; break;
            case Wire::ldBI:    alu.input_b = data_bus; break;
        }
        // clang-format on
    }

    // Write data_output_register back to RAM if the write signal is set - "in real life" this would happen at step 3 and
    // then the value would change if a new value were latched into the data output register at step 4, but that's not
    // important for this emulation.
    if (wires[Wire::sW])
    {
        system_bus = data_output_register;
        ram[address_bus_register.value] = system_bus;
    }
}

} // namespace Bootstrap
