#pragma once

#include "appstate.h"
#include "wad_loader.h"

#include <gli.h>

namespace fist
{

class TextureViewer : public AppState
{
public:
    void on_init(App* app) override;
    void on_pushed() override;
    void on_popped() override;
    void on_update(float delta) override;

private:
    App* _app{};
    gli::Sprite _texture{};
};

}