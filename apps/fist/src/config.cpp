#include "config.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace fist
{

void Config::load()
{
    std::ifstream fp("res/config.txt");

    if (fp)
    {
        std::string line;

        while (std::getline(fp, line))
        {
            std::string key;
            std::string value;
            size_t sep = line.find_first_of(':', 0);

            if (sep == std::string::npos)
            {
                continue;
            }

            key = line.substr(0, sep);
            key.erase(std::remove(key.begin(), key.end(), ' '), key.end());
            size_t vstart = line.find_first_not_of(' ', sep + 1);
            value = line.substr(vstart, std::string::npos);
            _data[key] = value;
        }
    }
}

void Config::save()
{
    std::ofstream fp("res/config.txt");

    if (fp)
    {
        std::vector<std::string> keys;
        keys.reserve(_data.size());
        std::transform(std::begin(_data), std::end(_data), std::back_inserter(keys),
                       [](const decltype(_data)::value_type& pair) { return pair.first; });
        std::sort(std::begin(keys), std::end(keys));

        for (const std::string& key : keys)
        {
            fp << key << " : " << _data[key] << std::endl;
        }
    }
}

float Config::get(const std::string& key, float default_value)
{
    float value = default_value;
    auto iter = _data.find(key);

    if (iter == _data.end())
    {
        _data.insert(std::pair<std::string, std::string>(key, std::to_string(default_value)));
    }
    else
    {
        value = std::stof(iter->second);
    }

    return value;
}

const std::string& Config::get(const std::string& key, const std::string& default_value)
{
    auto iter = _data.find(key);

    if (iter == _data.end())
    {
        _data.insert(std::pair<std::string, std::string>(key, default_value));
        iter = _data.find(key);
    }

    return iter->second;
}

} // namespace fist
