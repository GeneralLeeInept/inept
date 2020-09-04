#include "puzzle.h"

#include <memory>

namespace Bootstrap
{

namespace Puzzle
{

// clang-format off
Definition TestPuzzle{
    "Test Puzzle, not for resale",
    "LDA indirect",
    std::vector<Test>{
        // Test 0
        Test{
            // Initial state
            {
                1,  // PC = 1
                0,  // P = 0
                0,  // AC = 0
                0,  // X = 0
                std::vector<RamFragment>{
                    // RAM
                    RamFragment{ 1, std::vector<uint8_t>{ 0x80, 0x20 } },
                    RamFragment{ 0x2080, std::vector<uint8_t>{ 16 } }
                },
            },
            // Pass state
            {
                3,
                0,
                16,
                0,
                std::vector<RamFragment>{}
            }
        }
    }
};
// clang-format on

bool verify(const Definition& puzzle, const TgmCpu::Instruction& solution)
{
    std::unique_ptr<TgmCpu> cpu = std::make_unique<TgmCpu>();

    for (const Puzzle::Test& test : puzzle.tests)
    {
        memset(cpu->ram, 0, 64 * 1024);

        // Set initial state
        cpu->program_counter.value = test.initial_state.program_counter;
        cpu->status_register.value = test.initial_state.status_register;
        cpu->accumulator = test.initial_state.accumulator;
        cpu->index_register = test.initial_state.index_register;

        for (const RamFragment& frag : test.initial_state.ram_contents)
        {
            memcpy(&cpu->ram[frag.base_addr], &frag.data[0], frag.data.size());
        }

        cpu->execute(solution);

        // Check final state
        bool pass = true;
        pass = pass && (cpu->program_counter.value == test.pass_state.program_counter);
        pass = pass && (cpu->status_register.value == test.pass_state.status_register);
        pass = pass && (cpu->accumulator == test.pass_state.accumulator);
        pass = pass && (cpu->index_register == test.pass_state.index_register);


        for (const RamFragment& frag : test.pass_state.ram_contents)
        {
            pass = pass && (memcmp(&cpu->ram[frag.base_addr], &frag.data[0], frag.data.size()) == 0);
        }

        if (!pass)
        {
            return false;
        }
    }

    return true;
}


} // namespace Puzzle
} // namespace Bootstrap
