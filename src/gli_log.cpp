#include "gli_log.h"

#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace gli
{

void logf(const char* format, ...)
{
    std::va_list args;
    va_start(args, format);
    logv(format, args);
    va_end(args);
}


void logv(const char* format, va_list args)
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
        std::string buffer(len + 1, '\0');
        va_copy(args_copy, args);
        std::vsnprintf(&buffer[0], buffer.size(), format, args);
        va_end(args_copy);
        logm(buffer.data());
    }
}


void logm(const char* message)
{
    OutputDebugStringA(message);
}


static const char* log_level_string(int loglevel)
{
    if (loglevel == LogLevel::Debug)
    {
        return "Debug";
    }
    else if (loglevel == LogLevel::Info)
    {
        return "Info";
    }
    else if (loglevel == LogLevel::Warning)
    {
        return "Warning";
    }
    else if (loglevel == LogLevel::Error)
    {
        return "Error";
    }

    return "???";
}


static void print(std::string& buffer, const char* format, va_list args)
{
    static const size_t buffer_initial_size = 128;
    size_t len = 0;

    if (buffer.size() == 0)
    {
        buffer.resize(buffer_initial_size);
    }

    do
    {
        std::va_list args_copy;
        va_copy(args_copy, args);
        len = std::vsnprintf(&buffer[0], buffer.size(), format, args) + 1;
        va_end(args_copy);

        if (len > buffer.size())
        {
            buffer.resize(len);
        }
    } while (len > buffer.size());
}


static void print(std::string& buffer, const char* format, ...)
{
    std::va_list args;
    va_start(args, format);
    print(buffer, format, args);
    va_end(args);
}


void log(const char* file, int line, const char* category, int loglevel, const char* func, const char* format, ...)
{
    static std::string format_buffer;
    static std::string output_buffer;

    print(format_buffer, "%s(%d): %s: [%s] %s\n", file, line, category, log_level_string(loglevel), format);

    std::va_list args;
    va_start(args, format);
    print(output_buffer, format_buffer.c_str(), args);
    va_end(args);

    logm(output_buffer.c_str());
}

} // namespace gli