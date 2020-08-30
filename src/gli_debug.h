#pragma once

#include <assert.h>

#ifdef GLI_RELEASE
#define gliAssert(c) ((void)sizeof(c))
#else
void gli_assert(const char* file, int line, const char* message);
#define gliAssert(c) (void)((!!(c)) || (gli_assert(__FILE__, __LINE__, #c), 0))
#endif
