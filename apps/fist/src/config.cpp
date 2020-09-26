#include "config.h"

#include <fstream>
#include <string>

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
        for (const auto& kvp : _data)
        {
            fp << kvp.first << " : " << kvp.second << std::endl;
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
