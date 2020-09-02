#pragma once

#include <random>

namespace Bootstrap
{

class Random
{
public:
    Random();

    void reset();
    void reset(unsigned int seed);
    float get();
    float get(float min, float max);
    int get(int min, int max);

private:
    std::mt19937 _generator;
    std::uniform_int_distribution<> _int_distribution;
    std::uniform_real_distribution<float> _real_distribution;
};

extern Random gRandom;

} // namespace Bootstrap