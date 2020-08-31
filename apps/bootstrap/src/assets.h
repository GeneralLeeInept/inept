#pragma once

#if GLI_SHIPPING
#define GliAssetPath(p) R"(//assets.glp//)" p
#else
#define GliAssetPath(p) p
#endif
