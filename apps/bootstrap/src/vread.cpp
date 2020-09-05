#include "vread.h"

namespace Bootstrap
{

template <>
bool vread(std::string& s, std::vector<uint8_t>& data, size_t& read_ptr)
{
    uint16_t len;

    if (!vread(len, data, read_ptr))
    {
        return false;
    }

    if (len > data.size() - read_ptr)
    {
        return false;
    }

    s.resize(len);
    memcpy(&s[0], &data[read_ptr], len);
    read_ptr += len;
    return true;
}

} // namespace Bootstrap
