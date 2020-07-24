#include "gli_log.h"

#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>


void gli::logf(const char* format, ...)
{
    std::va_list args;
    va_start(args, format);
    logv(format, args);
    va_end(args);
}


void gli::logv(const char* format, va_list args)
{
    static constexpr int log_buffer_size = 256;
    static char log_buffer[log_buffer_size];
    std::va_list args_copy;
    va_copy(args_copy, args);
    int len = std::vsnprintf(log_buffer, log_buffer_size, format, args);
    va_end(args_copy);

    if (len < (log_buffer_size - 1))
    {
        logm(log_buffer);
    }
    else
    {
        std::vector<char> buffer(len + 1);
        va_copy(args_copy, args);
        std::vsnprintf(buffer.data(), buffer.size(), format, args);
        va_end(args_copy);
        logm(buffer.data());
    }
}


void gli::logm(const char* message)
{
    OutputDebugStringA(message);
}
