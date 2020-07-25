#include <gli.h>
#include <vga9.h>

#include "zmachine.h"

#include <deque>
#include <vector>

class Zilg : public gli::App
{
public:
    bool on_create() override
    {
        if (!fs.read_entire_file("//zork1.zip//DATA/ZORK1.DAT", story_data))
        {
            return false;
        }

        if (!zm.load(story_data))
        {
            return false;
        }

        return true;
    }

    void on_destroy() override
    {
    }

    void wrap_line_to_display(const std::string& line, std::vector<std::string>& display_lines, size_t line_length, size_t max_display_lines)
    {
        if (line.empty())
        {
            display_lines.push_back(line);
        }
        else
        {
            std::deque<std::pair<size_t, size_t>> wrapped_lines;
            size_t start = 0;
            size_t count = 0;
            size_t pos = 0;

            for (char c : line)
            {
                if (++pos == 120)
                {
                    wrapped_lines.push_front(std::pair<size_t, size_t>{ start, count - 1 });
                    start += count;
                    pos -= count;
                    count = 0;
                }

                if (c == ' ')
                {
                    count = pos;
                }
            }

            if (start < line.size())
            {
                wrapped_lines.push_front(std::pair<size_t, size_t>{ start, std::string::npos });
            }

            while (!wrapped_lines.empty() && display_lines.size() < max_display_lines)
            {
                std::pair<size_t, size_t>& wrapped_line = wrapped_lines.front();
                display_lines.push_back(line.substr(wrapped_line.first, wrapped_line.second));
                wrapped_lines.pop_front();
            }
        }
    }

    bool on_update(float delta) override
    {
        ZMachine::State state = zm.update();

        // Process keyboard input
        // TODO: Process keyboard events and send them directly to the machine
        // The machine can handle building up the input line (will also make read_char easy)
        for (int i = 0; i < 256; ++i)
        {
            if (m_keys[i].pressed)
            {
                if (i >= 0x20 && i <= 0x7F)
                {
                    input_buffer.push_back(i);
                }
                else if (i == VK_BACK && !input_buffer.empty())
                {
                    input_buffer.pop_back();
                }
                else if (i == VK_RETURN)
                {
                    zm.input(input_buffer);
                    gli::logf("User input: %s\n", input_buffer.c_str());
                    input_buffer.clear();
                }
            }
        }

        // Draw screen
        clear_screen(0);

        // Draw as many lines as we can fit (bottom up), first the input buffer and then lines from the transcript.
        std::vector<std::string>::const_reverse_iterator iter = zm.transcript().crbegin();
        std::vector<std::string>::const_reverse_iterator end = zm.transcript().crend();
        std::vector<std::string> display_lines;

        if (state == ZMachine::State::InputRequested)
        {
            std::string input_line = *iter;
            input_line += input_buffer;
            input_line += "_";
            wrap_line_to_display(input_line, display_lines, 80, 39);
            ++iter;
        }

        for (; iter != end && display_lines.size() < 39; ++iter)
        {
            wrap_line_to_display(*iter, display_lines, 80, 39);
        }

        int ypos = 8;
        for (std::vector<std::string>::const_reverse_iterator line = display_lines.crbegin(); line != display_lines.crend(); ++line)
        {
            draw_string(8, ypos, line->c_str(), (const int*)vga9_glyphs, vga9_glyph_width, vga9_glyph_height, 42, 0);
            ypos += vga9_glyph_height;
        }

        return true;
    }

    GliFileSystem fs;
    ZMachine zm;
    std::vector<uint8_t> story_data;
    std::string input_buffer;
};


Zilg zilg;

int gli_main(int argc, char** argv)
{
    if (zilg.initialize("ZILG - Can I offer you a Z-Machine Interpreter in these trying times?",
        vga9_glyph_width * 120 + 16, vga9_glyph_height * 40 + 16, 1))
    {
        zilg.run();
    }

    return 0;
}
