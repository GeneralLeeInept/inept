#include "gli_debug.h"
#include "gli_log.h"

#include <intrin.h>

void gli_assert(const char* file, int line, const char* message)
{
    gli::logf("%s(%d): ASSERT FAILED: %s\n", file, line, message);
    __debugbreak();
}
