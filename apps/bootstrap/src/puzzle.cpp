#include "puzzle.h"

#include "vread.h"

#include <gli.h>


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


constexpr uint32_t puzzle4cc()
{
    uint32_t magic = 0;
    return magic | ('P' << 24) | ('U' << 16) | ('Z' << 8) | 'Z';
}


bool load(Definition& puzzle, const std::string& path)
{
    puzzle = Definition();

    std::vector<uint8_t> data;

    if (!GliFileSystem::get()->read_entire_file(path.c_str(), data))
    {
        return false;
    }

    size_t read_ptr = 0;
    uint32_t magic;

    if (!vread(magic, data, read_ptr) || magic != puzzle4cc())
    {
        return false;
    }

    bool success = true;
    success = success && vread(puzzle.title, data, read_ptr);
    success = success && vread(puzzle.description, data, read_ptr);
    success = success && vread(puzzle.tests, data, read_ptr);

    return success;
}


bool verify(const Definition& puzzle, const TgmCpu::Instruction& solution)
{
    for (const Puzzle::Test& test : puzzle.tests)
    {
        TgmCpu cpu{};
        memset(cpu.ram, 0, 64 * 1024);

        // Set initial state
        cpu.program_counter.value = test.initial_state.program_counter;
        cpu.status_register.value = test.initial_state.status_register;
        cpu.accumulator = test.initial_state.accumulator;
        cpu.index_register = test.initial_state.index_register;

        for (const RamFragment& frag : test.initial_state.ram_contents)
        {
            memcpy(&cpu.ram[frag.base_addr], &frag.data[0], frag.data.size());
        }

        cpu.execute(solution);

        // Check final state
        bool pass = true;
        pass = pass && (cpu.program_counter.value == test.pass_state.program_counter);
        pass = pass && (cpu.status_register.value == test.pass_state.status_register);
        pass = pass && (cpu.accumulator == test.pass_state.accumulator);
        pass = pass && (cpu.index_register == test.pass_state.index_register);


        for (const RamFragment& frag : test.pass_state.ram_contents)
        {
            pass = pass && (memcmp(&cpu.ram[frag.base_addr], &frag.data[0], frag.data.size()) == 0);
        }

        if (!pass)
        {
            return false;
        }
    }

    return true;
}

} // namespace Puzzle

template <>
bool vread(Puzzle::RamFragment& f, std::vector<uint8_t>& data, size_t& read_ptr)
{
    bool result = true;
    result = result && vread(f.base_addr, data, read_ptr);
    result = result && vread(f.data, data, read_ptr);
    return result;
}


template <>
bool vread(Puzzle::CpuState& s, std::vector<uint8_t>& data, size_t& read_ptr)
{
    bool result = true;
    result = result && vread(s.program_counter, data, read_ptr);
    result = result && vread(s.status_register, data, read_ptr);
    result = result && vread(s.accumulator, data, read_ptr);
    result = result && vread(s.index_register, data, read_ptr);
    result = result && vread(s.ram_contents, data, read_ptr);
    return result;
}


template <>
bool vread(Puzzle::Test& t, std::vector<uint8_t>& data, size_t& read_ptr)
{
    bool result = true;
    result = result && vread(t.initial_state, data, read_ptr);
    result = result && vread(t.pass_state, data, read_ptr);
    return result;
}

} // namespace Bootstrap
