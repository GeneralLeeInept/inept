#pragma once

#include <cstdarg>

namespace gli
{

enum LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

void logf(const char* format, ...);
void logv(const char* format, va_list args);
void logm(const char* message);
void log(const char* file, int line, const char* category, int loglevel, const char* func, const char* format, ...);

} // namespace gli

#define GLI_LOGGING 1

#ifdef GLI_LOGGING
#define gliLog(loglevel, category, func, ...) gli::log(__FILE__, __LINE__, category, loglevel, func, __VA_ARGS__)
#else
#define gliLog(...) ((void)0)
#endif
