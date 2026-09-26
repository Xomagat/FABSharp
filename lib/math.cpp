//
// Created by Xomagat on 26.09.2026.
//

#include <cmath>

extern "C" double fab_pow(const double x, const double y)
{
    return std::pow(x, y);
}

extern "C" double fab_sqrt(const double x)
{
    return std::sqrt(x);
}