#include "random.h"

#include <cmath>

namespace Bootstrap
{


Random::Random()
{
    reset();
}


void Random::reset()
{
    _generator.seed(std::random_device()());
    _int_distribution.reset();
    _real_distribution.reset();
}


void Random::reset(unsigned int seed)
{
    _generator.seed(seed);
    _int_distribution.reset();
    _real_distribution.reset();
}


// Return a number in the range [0, 1]
float Random::get()
{
    return get(0.0f, std::nextafterf(1.0f, FLT_MAX));
}


// Return a number in the range [min, max)
float Random::get(float min, float max)
{
    using param_t = std::uniform_real_distribution<float>::param_type;
    return _real_distribution(_generator, param_t(min, max));
}


// Return a number in the range [min, max)
int Random::get(int min, int max)
{
    using param_t = std::uniform_int_distribution<>::param_type;
    return _int_distribution(_generator, param_t(min, max - 1));
}


Random gRandom;

} // namespace Bootstrap
