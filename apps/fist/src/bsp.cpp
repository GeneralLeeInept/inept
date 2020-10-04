#include "bsp.h"
#include "wad_loader.h"

namespace fist
{

Map::~Map()
{
    delete[] vertices;
    delete[] linedefs;
    delete[] sidedefs;
    delete[] sectors;
    delete[] linesegs;
    delete[] subsectors;
    delete[] nodes;
}

} // namespace fist