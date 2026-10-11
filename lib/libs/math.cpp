//
// Created by Xomagat on 26.09.2026.
//

#include <math.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" int _fltused = 0;

extern "C" int fab_absi(const int x)
{
    return abs(x);
}

extern "C" long fab_absl(const long x)
{
    return abs(x);
}

extern "C" float fab_ceilf(const float x)
{
    return ceil(x);
}

extern "C" double fab_ceild(const double x)
{
    return ceil(x);
}

extern "C" float fab_floorf(const float x)
{
    return floor(x);
}

extern "C" double fab_floord(const double x)
{
    return floor(x);
}

extern "C" float fab_fabsf(const float x)
{
    return fabs(x);
}

extern "C" double fab_fabsd(const double x)
{
    return fabs(x);
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
    return pow(x, y);
}

extern "C" float fab_roundf(const float x)
{
    if (x >= 8388608.0f || x <= -8388608.0f) return x;
    return (float)(int64_t)(x + (x >= 0.0f ? 0.5f : -0.5f));
}

extern "C" double fab_roundd(const double x)
{
    return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5);
}

extern "C" double fab_sqrtd(const double x)
{
    return sqrt(x);
}

extern "C" double fab_sqrti(const int x)
{
    return sqrt(x);
}