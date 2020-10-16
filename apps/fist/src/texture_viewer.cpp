#include "texture_viewer.h"

#include "app.h"

namespace fist
{

void TextureViewer::on_init(App* app)
{
    _app = app;
}

void TextureViewer::on_pushed()
{
    _app->config().load();
    const std::string& wad_location =
            _app->config().get("game.wadfile", R"(C:\Program Files (x86)\Steam\SteamApps\common\Ultimate Doom\base\DOOM.WAD)");
    const std::string& tex_name = _app->config().get("tv.texture", "STARG2");

    std::unique_ptr<WadFile, WadFileDeleter> wadfile(wad_open(wad_location));
    gliAssert(wadfile);

    Wad::Texture wadtexture;
    bool res = wad_load_texture(wadfile.get(), tex_name.c_str(), wadtexture);
    gliAssert(res);

    gli::Sprite texture(wadtexture.width, wadtexture.height, wadtexture.pixels.data());
    std::swap(_texture, texture);
}

void TextureViewer::on_popped()
{
}

void TextureViewer::on_update(float delta)
{
    _app->clear_screen(gli::Pixel(255, 0, 255));
    _app->blend_sprite(0, 0, _texture, 255);
}
} // namespace fist
