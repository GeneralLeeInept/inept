#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <array>
#include <string>
#include <fstream>
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

struct PngWrapper
{
    PngWrapper(const std::string& path)
    {
        data = stbi_load(path.c_str(), &w, &h, &comp, 0);
    }

    ~PngWrapper()
    {
        if (data)
        {
            stbi_image_free(data);
        }
    }

    int width() { return w; }
    int height() { return h; }
    int bpp() { return comp; }
    int stride() { return w * comp; }

    operator stbi_uc*() { return data; }
    operator bool() { return !!data; }

    stbi_uc operator()(int x, int y, int c)
    {
        if (data && x >= 0 && x < w && y >= 0 && y < h && c >= 0 && c < comp)
        {
            return data[y * stride() + x * comp + c];
        }
        return 0;
    }

    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_uc* data = nullptr;
};

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

    PngWrapper png(input);

    if (!png)
    {
        die([&]() { printf("Unable to load [%s]\n", input.c_str()); });
    }

    typedef std::array<int, 9 * 16> Glyph;
    typedef std::array<Glyph, 256> Font;
    Font* font = new Font();

    for (int i = 0; i < 256; ++i)
    {
        int gx = (i & 63) * 9;
        int gy = (i >> 6) * 16;
        int gi = 0;

        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 9; ++x)
            {
                (*font)[i][gi++] = !!png(gx + x, gy + y, 0);
            }
        }
    }

    std::ofstream of(output);

    if (!of)
    {
        die([&]() { printf("Unable to create output file [%s]\n", output.c_str()); });
    }

    of << "#pragma once\n\n";

    of << "static const int " << name << "_glyph_width = 9;\n";
    of << "static const int " << name << "_glyph_height = 16;\n";
    of << "static const int " << name << "_glyph_count = 256;\n";
    of << "static const int " << name << "_glyphs[" << name << "_glyph_width * " << name << "_glyph_height * " << name << "_glyph_count] = {";

    for (const Glyph& g : (*font))
    {
        of << "\n";

        for (int y = 0; y < 16; ++y)
        {
            of << "    ";
            for (int x = 0; x < 9; ++x)
            {
                of << g[y * 9 + x] << ",";
            }
            of << "\n";
        }
    }

    of << "};\n";

    of.close();

    delete font;

    return 0;
}
