#pragma once

#include <string>

#include "gli.h"

namespace Bootstrap
{

namespace SfxId
{

enum
{
    Ambience,
    DestroyEnemy,
    DestroyPlayer,
    NMI,
    Attack1,
    Attack2,
    MenuSelect,

    Count
};

}

struct SfxInfo
{
    std::string wavefile;
    float fade;
    bool looping;
};

bool load_sfx();
void get_sfx(int id, const SfxInfo*& info, const gli::WaveForm*& wave);

} // namespace Bootstrap
