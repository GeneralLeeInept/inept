#pragma once

#include "types.h"
#include "wad_loader.h"

#include <gli.h>

#include <unordered_map>
#include <string>
#include <vector>

namespace fist
{

class TextureManager
{
public:
    bool add_wad(const std::string& path);
    bool add_wad(WadFile* wad_file);
    gli::Sprite* get(uint64_t texture_id);

private:
    using Collection = std::vector<gli::Sprite>;
    using Lookup  = std::unordered_map<uint64_t, size_t>;
    Collection _textures{};
    Lookup _lookup{};
};

}
