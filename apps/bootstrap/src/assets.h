#pragma once

#if GLI_SHIPPING
#define GliAssetPath(p) R"(//assets.glp//)" p
#else
#define GliAssetPath(p) p
#endif

inline std::string asset_path(const std::string& path)
{
    // This is all because I messed up the filesystem - I don't want to have to special case every file path I use.
#if GLI_SHIPPING
    const std::string container = "//assets.glp//";
#else
    const std::string container;
#endif

    size_t size = std::snprintf(nullptr, 0, "%s%s", container.c_str(), path.c_str()) + 1;
    std::string buf(size, '\0');
    std::snprintf(&buf[0], size, "%s%s", container.c_str(), path.c_str());
    return buf;
}
