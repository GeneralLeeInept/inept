#include "sfx.h"

#include "assets.h"

namespace Bootstrap
{

static SfxInfo infos[SfxId::Count] = {
    // Ambience,
    { GliAssetPath("sfx/Ambience_Space_00.wav"), 1.0f, true },

    // DestroyEnemy,
    { GliAssetPath("sfx/Jingle_Achievement_00.wav"), 1.0f, false },

    // DestroyPlayer,
    { GliAssetPath("sfx/Jingle_Lose_00.wav"), 1.0f, false },

    // NMI,
    { GliAssetPath("sfx/Laser_00.wav"), 1.0f, false },

    // Attack1,
    { GliAssetPath("sfx/Laser_01.wav"), 1.0f, false },

    // Attack2,
    { GliAssetPath("sfx/Laser_02.wav"), 1.0f, false },

    // MenuSelect,
    { GliAssetPath("sfx/Menu_Select_00.wav"), 1.0f, false },
};

static gli::WaveForm waveforms[SfxId::Count];


bool load_sfx()
{
    size_t i = 0;

    for (const SfxInfo& info : infos)
    {
        if (!waveforms[i++].load(info.wavefile))
        {
            return false;
        }
    }

    return true;
}


void get_sfx(int id, const SfxInfo*& info, const gli::WaveForm*& wave)
{
    if (id >= 0 && id < SfxId::Count)
    {
        info = &infos[id];
        wave = &waveforms[id];
    }
    else
    {
        gliLog(gli::LogLevel::Error, "Sfx", "get_sfx", "Invalid SFX ID.");
        info = nullptr;
        wave = nullptr;
    }
}

}
