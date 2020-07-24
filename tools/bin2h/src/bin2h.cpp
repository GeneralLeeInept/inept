#include <stdio.h>
#include <stdlib.h>

#include <array>
#include <string>
#include <fstream>
#include <iterator>
#include <vector>


template<typename F>
void die(const F& f)
{
    f();
    exit(1);
}


void usage()
{
    printf("Usage:\n");
    printf("\tfontgen -o ouputfile -n name inputfile\n");
}


bool read_entire_file(const std::string& path, std::vector<uint8_t>& contents)
{
    std::ifstream ifs(path, std::ios::binary);

    if (!ifs)
        return false;

    auto begin = std::istreambuf_iterator<char>(ifs);
    auto end = std::istreambuf_iterator<char>();
    contents = std::move(std::vector<uint8_t>(begin, end));
    return true;
}


int main(int argc, char** argv)
{
    std::string input;
    std::string output;
    std::string name;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);

        if (arg[0] == '-')
        {
            if (arg == "-o" && output.empty())
            {
                if (++i == argc)
                {
                    die(usage);
                }

                output = argv[i];
            }
            else if (arg == "-n" && name.empty())
            {
                if (++i == argc)
                {
                    die(usage);
                }

                name = argv[i];
            }
            else
            {
                die(usage);
            }
        }
        else if (input.empty())
        {
            input = arg;
        }
        else
        {
            die(usage);
        }
    }

    if (input.empty() || output.empty() || name.empty())
    {
        die(usage);
    }

    std::vector<uint8_t> binary_data;

    if (!read_entire_file(input, binary_data))
    {
        die([&]() { printf("Unable to read input file [%s]\n", output.c_str()); });
    }

    std::ofstream ofs(output);

    if (!ofs)
    {
        die([&]() { printf("Unable to create output file [%s]\n", output.c_str()); });
    }

    ofs << "#pragma once\n\n";

    ofs << "#include <cstdint>\n\n";

    ofs << "static const uint8_t " << name << "[" << binary_data.size() << "] = {\n";

    uint8_t *byte = binary_data.data();
    size_t remaining = binary_data.size();
    char line[256];

    for (; remaining > 23; remaining -= 23)
    {
        char* dest = line;
        dest += std::snprintf(line, 256, "   ");

        for (int i = 0; i < 23; ++i)
        {
            dest += std::snprintf(dest, 256 - (dest - line), " 0x%02X,", *byte++);
        }

        ofs << line << "\n";
    }

    if (remaining)
    {
        char* dest = line;
        dest += std::snprintf(line, 256, "   ");

        for (int i = 0; i < remaining; ++i)
        {
            dest += std::snprintf(dest, 256 - (dest - line), " 0x%02X,", *byte++);
        }

        ofs << line << "\n";
    }

    ofs << "};\n";

    ofs.close();

    return 0;
}
