#pragma once

#include <string>
#include <vector>

namespace DeadPlanet
{

template <typename T>
bool vread(T& output, std::vector<uint8_t>& data, size_t& read_ptr)
{
    if (sizeof(T) > data.size() - read_ptr)
    {
        return false;
    }

    memcpy(&output, &data[read_ptr], sizeof(T));
    read_ptr += sizeof(T);
    return true;
}


template <typename T>
bool vread(std::vector<T>& v, size_t count, std::vector<uint8_t>& data, size_t& read_ptr)
{
    v.resize(count);

    bool result = true;

    for (auto& t : v)
    {
        result = vread(t, data, read_ptr);

        if (!result)
        {
            break;
        }
    }

    return result;
}


template <typename T>
bool vread(std::vector<T>& v, std::vector<uint8_t>& data, size_t& read_ptr)
{
    uint16_t count;

    if (!vread(count, data, read_ptr))
    {
        return false;
    }

    return vread(v, count, data, read_ptr);
}

template <>
bool vread(std::string& s, std::vector<uint8_t>& data, size_t& read_ptr);

} // namespace DeadPlanet