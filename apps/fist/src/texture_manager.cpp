#include "texture_manager.h"

namespace fist
{

bool TextureManager::add_wad(const std::string& path)
{
    // Load all the textures in the wad and add them to the lookup
    std::unique_ptr<WadFile, WadFileDeleter> wadfile(wad_open(path));

    if (!wadfile)
    {
        gliLog(gli::LogLevel::Error, "TextureManager", "TextureManager::add_wad", "Failed to load wad file '%s'", path.c_str());
        return false;
    }

    return add_wad(wadfile.get());
}

bool TextureManager::add_wad(WadFile* wad_file)
{
    auto load_callback = [](int64_t id, const Wad::Texture& texture, void* user_data) {
        TextureManager* tm = (TextureManager*)user_data;
        tm->_lookup[id] = tm->_textures.size();
        tm->_textures.push_back(gli::Sprite(texture.width, texture.height, texture.pixels.data()));
    };

    wad_load_all_textures(wad_file, load_callback, this);

    return true;
}

gli::Sprite* TextureManager::get(uint64_t texture_id)
{
    gli::Sprite* tex = nullptr; 
    Lookup::iterator it = _lookup.find(texture_id);

    if (it != _lookup.end())
    {
        tex = &_textures[it->second];
    }

    return tex;
}

}