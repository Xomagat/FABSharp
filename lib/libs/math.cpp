//
// Created by Xomagat on 26.09.2026.
//

#include <math.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" int _fltused = 0;

// Generated: double (d) and float (f) versions through libm
#define FAB_MATH_D(NAME) \
    extern "C" double fab_##NAME##d(const double x) { return NAME(x); }

#define FAB_MATH_F(NAME) \
    extern "C" float fab_##NAME##f(const float x) { return NAME##f(x); }

#define FAB_MATH(NAME) FAB_MATH_D(NAME) FAB_MATH_F(NAME)

// Abs
extern "C" int fab_absi(const int x)
{
    return x < 0 ? -x : x;
}

extern "C" long long fab_absl(const long long x)
{
    return x < 0 ? -x : x;
}

extern "C" double fab_absd(const double x)
{
    return fabs(x);
}

FAB_MATH(fabs)

// Rounding
FAB_MATH(ceil)
FAB_MATH(floor)

extern "C" double fab_fmodd(const double a, const double b) { return fmod(a, b); }
extern "C" float  fab_fmodf(const float a, const float b)   { return fmodf(a, b); }

// Result has the sign of b (unlike %)
extern "C" int fab_modi(const int a, const int b)
{
    if (b == 0) return 0;
    int r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
}

// Pow (integers: fast pow, O(log y))
extern "C" int fab_powi(int x, int y)
{
    if (y < 0) return 0;
    int result = 1;
    while (y)
    {
        if (y & 1) result *= x;
        x *= x;
        y >>= 1;
    }
    return result;
}

extern "C" long long fab_powl(long long x, long long y)
{
    if (y < 0) return 0;
    long long result = 1;
    while (y)
    {
        if (y & 1) result *= x;
        x *= x;
        y >>= 1;
    }
    return result;
}

extern "C" double fab_powd(const double x, const double y) { return pow(x, y); }
extern "C" float  fab_powf(const float x, const float y)   { return powf(x, y); }

// Sqrt, cbrt, hypot
extern "C" double fab_sqrti(const int x) { return sqrt(x); }

FAB_MATH(sqrt)

extern "C" double fab_cbrtd(const double x)
{
    if (x == 0.0 || x != x || x - x != 0.0) return x;
    double a = x < 0 ? -x : x;
    double y = pow(a, 1.0 / 3.0);
    y -= (y * y * y - a) / (3.0 * y * y);      // one Newton step removes the last-digit error
    return x < 0 ? -y : y;
}
extern "C" float fab_cbrtf(const float x) { return (float)fab_cbrtd(x); }

extern "C" double fab_hypotd(const double a, const double b)
{
    double x = a < 0 ? -a : a;
    double y = b < 0 ? -b : b;
    if ((x == x && x - x != 0.0) || (y == y && y - y != 0.0)) return HUGE_VAL;  // inf wins over NaN
    if (x < y) { double t = x; x = y; y = t; }
    if (x == 0.0) return 0.0;
    double r = y / x;
    return x * sqrt(1.0 + r * r);
}
extern "C" float fab_hypotf(const float a, const float b) { return (float)fab_hypotd(a, b); }

// Trigonometry
FAB_MATH(sin)   FAB_MATH(cos)   FAB_MATH(tan)
FAB_MATH(asin)  FAB_MATH(acos)  FAB_MATH(atan)
FAB_MATH(sinh)  FAB_MATH(cosh)  FAB_MATH(tanh)

extern "C" double fab_atan2d(const double y, const double x) { return atan2(y, x); }
extern "C" float  fab_atan2f(const float y, const float x)   { return atan2f(y, x); }

// Exp, log
FAB_MATH(exp)
FAB_MATH(log)
FAB_MATH(log2)
FAB_MATH(log10)

// Round half away from zero, without libm round()
extern "C" double fab_roundd(const double x)
{
    if (x != x || x - x != 0.0) return x;      // NaN, inf
    double a = x < 0 ? -x : x;
    double r = floor(a);
    if (a - r >= 0.5) r += 1.0;
    return x < 0 ? -r : r;
}
extern "C" float fab_roundf(const float x) { return (float)fab_roundd(x); }

extern "C" double fab_truncd(const double x) { return x < 0 ? ceil(x) : floor(x); }
extern "C" float  fab_truncf(const float x)  { return (float)fab_truncd(x); }

extern "C" double fab_logbased(const double x, const double base)
{
    return log(x) / log(base);
}

// Min, max, clamp, sign
#define FAB_MINMAX(S, T) \
    extern "C" T   fab_min##S(const T a, const T b)                { return a < b ? a : b; } \
    extern "C" T   fab_max##S(const T a, const T b)                { return a > b ? a : b; } \
    extern "C" T   fab_clamp##S(const T x, const T lo, const T hi) { return x < lo ? lo : (x > hi ? hi : x); } \
    extern "C" int fab_sign##S(const T x)                          { return (x > 0) - (x < 0); }

FAB_MINMAX(i, int)
FAB_MINMAX(l, long long)
FAB_MINMAX(f, float)
FAB_MINMAX(d, double)

// Integer math
extern "C" int fab_gcdi(int a, int b)
{
    while (b) { int t = a % b; a = b; b = t; }
    return a < 0 ? -a : a;
}

extern "C" long long fab_gcdl(long long a, long long b)
{
    while (b) { long long t = a % b; a = b; b = t; }
    return a < 0 ? -a : a;
}

extern "C" int fab_lcmi(const int a, const int b)
{
    if (a == 0 || b == 0) return 0;
    int r = (a / fab_gcdi(a, b)) * b;
    return r < 0 ? -r : r;
}

extern "C" long long fab_lcml(const long long a, const long long b)
{
    if (a == 0 || b == 0) return 0;
    long long r = (a / fab_gcdl(a, b)) * b;
    return r < 0 ? -r : r;
}

// n > 20 does not fit in long long, n < 0 is undefined -> -1
extern "C" long long fab_factoriall(const int n)
{
    if (n < 0 || n > 20) return -1;
    long long r = 1;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}

extern "C" bool fab_is_primel(const long long n)
{
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (long long i = 5; i * i <= n; i += 6)
        if (n % i == 0 || n % (i + 2) == 0) return false;
    return true;
}

extern "C" bool fab_is_primei(const int n) { return fab_is_primel(n); }

// Value checks
extern "C" bool fab_is_nand(const double x)    { return x != x; }
extern "C" bool fab_is_nanf(const float x)     { return x != x; }
extern "C" bool fab_is_infd(const double x)    { return x == x && (x - x) != 0.0; }
extern "C" bool fab_is_inff(const float x)     { return x == x && (x - x) != 0.0f; }
extern "C" bool fab_is_finited(const double x) { return (x - x) == 0.0; }
extern "C" bool fab_is_finitef(const float x)  { return (x - x) == 0.0f; }

extern "C" double fab_infd() { return HUGE_VAL; }
extern "C" double fab_nand() { return NAN; }

// Angles and interpolation
extern "C" double fab_to_radd(const double deg) { return deg * 3.14159265358979323846 / 180.0; }
extern "C" double fab_to_degd(const double rad) { return rad * 180.0 / 3.14159265358979323846; }
extern "C" float  fab_to_radf(const float deg)  { return deg * (float)(3.14159265358979323846 / 180.0); }
extern "C" float  fab_to_degf(const float rad)  { return rad * (float)(180.0 / 3.14159265358979323846); }

extern "C" double fab_lerpd(const double a, const double b, const double t) { return a + (b - a) * t; }
extern "C" float  fab_lerpf(const float a, const float b, const float t)    { return a + (b - a) * t; }