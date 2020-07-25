#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>


namespace InterpreterFlags
{
typedef uint8_t Type;

enum Bits3
{
    StatusLineType = 0x02,
    SplitStoryFile = 0x04,
    NoStatusLine = 0x10,
    ScreenSplitSupported = 0x20,
    ProportionalFont = 0x40
};

enum Bits4
{
    BoldfaceSupported = 0x04,
    ItalicSupported = 0x08,
    MonospaceSupported = 0x10,
    TimedInputSupported = 0x80
};

enum Bits5
{
    ColorsSupported = 0x01
};

enum Bits6
{
    PicturesSupported = 0x02,
    SoundSupported = 0x20
};

} // namespace InterpreterFlags


namespace GameFlags
{
typedef uint16_t Type;

enum Bits3
{
    Transcripting = 0x0001,
    ForceMonospace = 0x0002
};

enum Bits5
{
    PicturesRequested = 0x0008,
    UndoRequested = 0x0010,
    MouseRequested = 0x0020,
    ColorsRequested = 0x0040,
    SoundRequested = 0x0080,
    MenuRequested = 0x0100
};

enum Bits6
{
    RedrawRequested = 0x04
};
} // namespace GameFlags


struct ZMachineHeader
{
    uint8_t version;
    InterpreterFlags::Type interpreter_flags;
    uint16_t release_number;
    uint16_t high_mem_base;
    uint16_t initial_pc;
    uint16_t dictionary_table;
    uint16_t object_table;
    uint16_t globals_table;
    uint16_t static_mem_base;
    GameFlags::Type game_flags;
    uint8_t serial[6];
    uint16_t abbreviations_table;
    uint16_t file_length;
    uint16_t file_checksum;
    uint8_t interpreter_number;
    uint8_t interpreter_version;
    uint8_t screen_height_chars;
    uint8_t screen_width_chars;
    uint16_t screen_width_units;
    uint16_t screen_height_units;
    uint8_t font_width_units;
    uint8_t font_height_units;
    uint16_t routines_offset;
    uint16_t static_strings_offset;
    uint8_t default_background_color;
    uint8_t default_foreground_color;
    uint16_t terminating_characters_table;
    uint16_t stream3_output_width;
    uint16_t standard_revision_number;
    uint16_t alphabet_table;
    uint16_t extension_table;
};


inline uint16_t swap_endian(uint16_t x)
{
    return ((x & 0xFF00) >> 8) | ((x & 0x00FF) << 8);
}


inline uint8_t lo(uint16_t word)
{
    return (word & 0xFF);
}


inline uint8_t hi(uint16_t word)
{
    return (word >> 8);
}


inline uint16_t make_word(uint8_t msb, uint8_t lsb)
{
    return (msb << 8) | lsb;
}


inline uint16_t lo(uint32_t address)
{
    return (address & 0xFFFF);
}


inline uint16_t hi(uint32_t address)
{
    return (address >> 16);
}


inline uint32_t make_address(uint16_t msw, uint16_t lsw)
{
    return (msw << 16) | lsw;
}


class ZMachine
{
public:
    enum class State : int
    {
        Crashed = -1,
        Running,
        InputRequested
    };

    enum OperandTypes
    {
        OpImm16,
        OpImm8,
        OpVar
    };

    struct ZInstruction
    {
        uint16_t opcode;
        uint8_t operand_count;
        uint8_t operand_types[8];
        uint16_t operands[8];
    };

    using InstructionHandler = void (ZMachine::*)(ZInstruction&);
    using InstructionHandlers = std::unordered_map<uint16_t, InstructionHandler>;
    using InstructionMnemonics = std::unordered_map<uint16_t, const char*>;

    struct ObjectTraits
    {
        uint8_t object_index_size_bytes;    // maximum objects is (2^object_index_size_bytes) - 1
        uint8_t object_size_bytes;
        uint8_t attribute_flag_bytes;
        uint8_t max_properties;
    };

    struct Traits
    {
        InstructionHandlers instruction_handlers;
        InstructionMnemonics instruction_mnemonics;
        uint8_t paddr_offset_scale;
        uint8_t paddr_base_scale;
        uint8_t dictionary_word_length; // Number of 16-bit words stored for each word in dictionary
        ObjectTraits object_traits;
    };

    ZMachine() = default;
    ~ZMachine();

    bool load(std::vector<uint8_t>& story_file);
    void reset();
    State state() { return _current_state; }

    State update();

    const std::vector<std::string>& transcript() { return _transcript; }
    void input(const std::string& user_input);

    // Read and write memory
    uint8_t read(uint32_t addr);
    void write(uint32_t addr, uint8_t byte);

    uint16_t readw(uint32_t addr);
    void writew(uint32_t addr, uint16_t word);

    uint8_t read_table(uint32_t addr, uint16_t index);
    void write_table(uint32_t addr, uint16_t index, uint8_t byte);

    uint16_t read_tablew(uint32_t addr, uint16_t index);
    void write_tablew(uint32_t addr, uint16_t index, uint16_t word);

    uint16_t read_string(uint32_t addr, std::vector<char>& str, bool terminate);

    // Read and write variables
    uint16_t readv(uint8_t var);
    void writev(uint8_t var, uint16_t value);

    // Stack operations
    void push(uint16_t value);
    uint16_t pop();

    void pusha(uint32_t address);
    uint32_t popa();

    void push_stack_frame();
    void pop_stack_frame();

    // Objects & properties
    uint16_t get_object_ptr(uint16_t object_index);

    uint8_t get_attribute(uint16_t object_index, uint8_t attribute_index);
    void set_attribute(uint16_t object_index, uint8_t attribute_index, uint8_t value);

    uint16_t get_parent(uint16_t object_index);
    void set_parent(uint16_t object_index, uint16_t parent_index);

    uint16_t get_sibling(uint16_t object_index);
    void set_sibling(uint16_t object_index, uint16_t sibling_index);

    uint16_t get_child(uint16_t object_index);
    void set_child(uint16_t object_index, uint16_t child_index);

    void get_object_short_name(uint16_t object_index, std::vector<char>& str);

    uint16_t get_prop_addr(uint16_t object_index, uint8_t property_index);
    uint8_t get_prop_len(uint16_t prop_addr);
    uint8_t get_prop_index(uint16_t prop_addr);

    uint16_t get_prop(uint16_t object_index, uint8_t property_index);
    void put_prop(uint16_t object_index, uint8_t property_index, uint16_t value);

    uint8_t get_next_prop_index(uint16_t object_index, uint8_t property_index);

    // Miscellaneous
    void store_result(uint16_t value);
    void apply_predicate(bool test);
    void ret(uint16_t result);
    bool zscii_to_ascii(char& ascii_code, uint16_t zscii_code, bool for_output);
    const char* mnemonic(uint16_t opcode);
    uint32_t unpack_paddr(uint16_t paddr, bool string);
    void parse(uint16_t text_buffer, uint16_t parse_buffer);

    // 2OP Instruction handlers
    void _je(ZInstruction&);
    void _jl(ZInstruction&);
    void _jg(ZInstruction&);
    void _dec_chk(ZInstruction&);
    void _inc_chk(ZInstruction&);
    void _jin(ZInstruction&);
    void _test(ZInstruction&);
    void _or(ZInstruction&);
    void _and(ZInstruction&);
    void _test_attr(ZInstruction&);
    void _set_attr(ZInstruction&);
    void _clear_attr(ZInstruction&);
    void _store(ZInstruction&);
    void _insert_obj(ZInstruction&);
    void _loadw(ZInstruction&);
    void _loadb(ZInstruction&);
    void _get_prop(ZInstruction&);
    void _get_prop_addr(ZInstruction&);
    void _get_next_prop(ZInstruction&);
    void _add(ZInstruction&);
    void _sub(ZInstruction&);
    void _mul(ZInstruction&);
    void _div(ZInstruction&);
    void _mod(ZInstruction&);
    void _call_2s_4(ZInstruction&);
    void _call_2n_5(ZInstruction&);
    void _set_colour_5(ZInstruction&);
    void _set_colour_6(ZInstruction&);
    void _throw_5(ZInstruction&);

    // 1OP Instruction handlers
    void _jz(ZInstruction&);
    void _get_sibling(ZInstruction&);
    void _get_child(ZInstruction&);
    void _get_parent(ZInstruction&);
    void _get_prop_len(ZInstruction&);
    void _inc(ZInstruction&);
    void _dec(ZInstruction&);
    void _print_addr(ZInstruction&);
    void _call_1s_4(ZInstruction&);
    void _remove_obj(ZInstruction&);
    void _print_obj(ZInstruction&);
    void _ret(ZInstruction&);
    void _jump(ZInstruction&);
    void _print_paddr(ZInstruction&);
    void _load(ZInstruction&);
    void _not(ZInstruction&);
    void _call_1n_5(ZInstruction&);

    // 0OP Instruction handlers
    void _rtrue(ZInstruction&);
    void _rfalse(ZInstruction&);
    void _print(ZInstruction&);
    void _print_ret(ZInstruction&);
    void _nop(ZInstruction&);
    void _save(ZInstruction&);
    void _save_4(ZInstruction&);
    void _restore(ZInstruction&);
    void _restore_4(ZInstruction&);
    void _restart(ZInstruction&);
    void _ret_popped(ZInstruction&);
    void _pop(ZInstruction&);
    void _catch_5(ZInstruction&);
    void _quit(ZInstruction&);
    void _new_line(ZInstruction&);
    void _show_status(ZInstruction&);
    void _verify(ZInstruction&);
    void _extended_5(ZInstruction&);
    void _piracy_5(ZInstruction&);

    // EXT/VAR Instruction handlers
    void _call(ZInstruction&);
    void _call_vs_4(ZInstruction&);
    void _storew(ZInstruction&);
    void _storeb(ZInstruction&);
    void _put_prop(ZInstruction&);
    void _sread(ZInstruction&);
    void _sread_4(ZInstruction&);
    void _aread_5(ZInstruction&);
    void _print_char(ZInstruction&);
    void _print_num(ZInstruction&);
    void _random(ZInstruction&);
    void _push(ZInstruction&);
    void _pull(ZInstruction&);
    void _pull_6(ZInstruction&);
    void _split_window(ZInstruction&);
    void _set_window(ZInstruction&);
    void _call_vs2_4(ZInstruction&);
    void _erase_window_4(ZInstruction&);
    void _erase_line_4(ZInstruction&);
    void _erase_line_6(ZInstruction&);
    void _set_cursor_4(ZInstruction&);
    void _set_cursor_6(ZInstruction&);
    void _get_cursor_4(ZInstruction&);
    void _set_text_style_4(ZInstruction&);
    void _buffer_mode_4(ZInstruction&);
    void _output_stream(ZInstruction&);
    void _output_stream_5(ZInstruction&);
    void _output_stream_6(ZInstruction&);
    void _input_stream(ZInstruction&);
    void _sound_effect_5(ZInstruction&);
    void _read_char_4(ZInstruction&);
    void _scan_table_4(ZInstruction&);
    void _not_5(ZInstruction&);
    void _call_vn_5(ZInstruction&);
    void _call_vn2_5(ZInstruction&);
    void _tokenise_5(ZInstruction&);
    void _encode_text_5(ZInstruction&);
    void _copy_table_5(ZInstruction&);
    void _print_table_5(ZInstruction&);
    void _check_arg_count_5(ZInstruction&);

private:
    Traits _traits{};
    uint8_t* _memory = nullptr;
    uint32_t _memory_size = 0;
    uint16_t _stack[64 * 1024];
    ZMachineHeader _header{};
    uint32_t _pc{};
    uint32_t _resume_pc{};
    uint16_t _sp{};
    uint16_t _locals_base{};
    State _current_state = State::Crashed;
    std::stringstream _linebuffer{};
    std::vector<std::string> _transcript{};
    std::deque<std::string> _user_input{};
    uint16_t _random_state{};

    void flush_line();
    void crash(const char* format, ...);
    void set_state(State state);
};
