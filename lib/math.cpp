//
// Created by Xomagat on 26.09.2026.
//

#include <cmath>

extern "C" int fab_absi(const int x)
{
    return std::abs(x);
}

extern "C" long fab_absl(const long x)
{
    return std::abs(x);
}

extern "C" double fab_absd(const double x)
{
    return std::abs(x);
}

extern "C" float fab_ceilf(const float x)
{
    return std::ceil(x);
}

extern "C" double fab_ceild(const double x)
{
    return std::ceil(x);
}

extern "C" float fab_floorf(const float x)
{
    return std::floor(x);
}

extern "C" double fab_floord(const double x)
{
    return std::floor(x);
}

extern "C" float fab_fabsf(const float x)
{
    return std::fabs(x);
}

extern "C" double fab_fabsd(const double x)
{
    return std::fabs(x);
}

extern "C" int fab_powi(const int x, const int y)
{
    int result = 1;
    for (int i = 0; i < y; ++i) result *= x;
    return result;
}

extern "C" long fab_powl(const long x, const long y)
{
    long result = 1;
    for (long i = 0; i < y; ++i) result *= x;
    return result;
}

extern "C" double fab_powd(const double x, const double y)
{
    return std::pow(x, y);
}

extern "C" float fab_roundf(const float x)
{
    return std::round(x);
}

extern "C" double fab_roundd(const double x)
{
    return (x >= 0.0) ? std::floor(x + 0.5) : std::ceil(x - 0.5);
}

extern "C" double fab_sqrtd(const double x)
{
    return std::sqrt(x);
}

extern "C" double fab_sqrti(const int x)
{
    return std::sqrt(x);
}