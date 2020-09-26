#pragma once

#include <unordered_map>

namespace fist
{

class Config
{
public:
    Config() = default;

    void load();
    void save();

    float get(const std::string& key, float default_value);
    const std::string& get(const std::string& key, const std::string& default_value);

private:
    std::unordered_map<std::string, std::string> _data{};
};

} // namespace fist
