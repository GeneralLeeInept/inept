#pragma once

#include <string>
#include <vector>

#include "tgmcpu.h"

namespace Bootstrap
{

namespace Puzzle
{

struct RamFragment
{
    uint16_t base_addr;
    std::vector<uint8_t> data;
};


struct CpuState
{
    uint16_t program_counter;
    uint8_t status_register;
    uint8_t accumulator;
    uint8_t index_register;
    std::vector<RamFragment> ram_contents;
};


struct Test
{
    CpuState initial_state;
    CpuState pass_state;
};


struct Definition
{
    std::string title;
    std::string description;
    std::vector<Test> tests;
};


bool verify(const Definition& puzzle, const TgmCpu::Instruction& solution);

extern Definition TestPuzzle;

} // namespace Puzzle
} // namespace Bootstrap