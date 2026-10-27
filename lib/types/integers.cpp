//
// Created by Xomagat on 08.10.2026.
//

//
// Created by Xomagat on 08.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Helper: format a value into a freshly malloc'ed string
static char* fab_fmt_alloc(const char* fmt, long long iv, double dv, bool is_float)
{
    int size = is_float ? snprintf(nullptr, 0, fmt, dv) : snprintf(nullptr, 0, fmt, iv);
    if (size < 0) return nullptr;

    char* p = (char*)malloc((size_t)size + 1);
    if (p == nullptr) return nullptr;

    if (is_float) snprintf(p, (size_t)size + 1, fmt, dv);
    else          snprintf(p, (size_t)size + 1, fmt, iv);

    return p;
}

// Same for long double (x87, 80 bit): needs %Lg
static char* fab_fmt_alloc_ld(const char* fmt, long double x)
{
    int size = snprintf(nullptr, 0, fmt, x);
    if (size < 0) return nullptr;

    char* p = (char*)malloc((size_t)size + 1);
    if (p == nullptr) return nullptr;

    snprintf(p, (size_t)size + 1, fmt, x);
    return p;
}

// Integer types: short, int, long
// NAME - suffix, T - C type, FMT - printf format for long long
#define FAB_INTEGER(NAME, T) \
    extern "C" char* fab_to_str_##NAME(T x)         { return fab_fmt_alloc("%lld", (long long)x, 0.0, false); } \
    extern "C" bool  fab_is_positive_##NAME(T x)    { return x > 0; } \
    extern "C" bool  fab_is_negative_##NAME(T x)    { return x < 0; } \
    extern "C" bool  fab_is_zero_##NAME(T x)        { return x == 0; } \
    extern "C" bool  fab_is_even_##NAME(T x)        { return (x % 2) == 0; } \
    extern "C" bool  fab_is_odd_##NAME(T x)         { return (x % 2) != 0; } \
    extern "C" T     fab_abs_##NAME(T x)            { return x < 0 ? (T)-x : x; } \
    extern "C" int   fab_sign_##NAME(T x)           { return (x > 0) - (x < 0); } \
    extern "C" bool  fab_to_bool_##NAME(T x)        { return x != 0; } \
    extern "C" char  fab_to_char_##NAME(T x)        { return (char)x; } \
    extern "C" short fab_to_short_##NAME(T x)       { return (short)x; } \
    extern "C" int   fab_to_int_##NAME(T x)         { return (int)x; } \
    extern "C" long long fab_to_long_##NAME(T x)    { return (long long)x; } \
    extern "C" float fab_to_float_##NAME(T x)       { return (float)x; } \
    extern "C" double fab_to_double_##NAME(T x)     { return (double)x; } \
    extern "C" long double fab_to_ldouble_##NAME(T x) { return (long double)x; }

// Int
FAB_INTEGER(int, int)

// Short
FAB_INTEGER(short, short)

// Long
FAB_INTEGER(long, long long)

// Float and double
// Conversion to integers: NaN -> 0, out of range -> saturation
static long long fab_sat(double x, long long lo, long long hi)
{
    if (x != x) return 0;
    if (x <= (double)lo) return lo;
    if (x >= (double)hi) return hi;
    return (long long)x;
}

// FMT - printf format: "%.9g" for float, "%.17g" for double
#define FAB_FLOATING(NAME, T, FMT) \
    extern "C" char* fab_to_str_##NAME(T x)         { return fab_fmt_alloc(FMT, 0, (double)x, true); } \
    extern "C" bool  fab_is_positive_##NAME(T x)    { return x > 0; } \
    extern "C" bool  fab_is_negative_##NAME(T x)    { return x < 0; } \
    extern "C" bool  fab_is_zero_##NAME(T x)        { return x == 0; } \
    extern "C" bool  fab_is_nan_##NAME(T x)         { return x != x; } \
    extern "C" bool  fab_is_inf_##NAME(T x)         { return x == x && (x - x) != 0; } \
    extern "C" bool  fab_is_finite_##NAME(T x)      { return (x - x) == 0; } \
    extern "C" T     fab_abs_##NAME(T x)            { return x < 0 ? -x : x; } \
    extern "C" int   fab_sign_##NAME(T x)           { return (x > 0) - (x < 0); } \
    extern "C" T     fab_floor_##NAME(T x)          { return (T)floor((double)x); } \
    extern "C" T     fab_ceil_##NAME(T x)           { return (T)ceil((double)x); } \
    extern "C" T     fab_trunc_##NAME(T x)          { return (T)(x < 0 ? ceil((double)x) : floor((double)x)); } \
    extern "C" T     fab_round_##NAME(T x)          { return (T)(x < 0 ? -floor(-(double)x + 0.5) : floor((double)x + 0.5)); } \
    extern "C" bool  fab_to_bool_##NAME(T x)        { return x != 0; } \
    extern "C" char  fab_to_char_##NAME(T x)        { return (char)fab_sat((double)x, -128LL, 127LL); } \
    extern "C" short fab_to_short_##NAME(T x)       { return (short)fab_sat((double)x, -32768LL, 32767LL); } \
    extern "C" int   fab_to_int_##NAME(T x)         { return (int)fab_sat((double)x, -2147483648LL, 2147483647LL); } \
    extern "C" long long fab_to_long_##NAME(T x)    { return fab_sat((double)x, (-9223372036854775807LL - 1), 9223372036854775807LL); }

// Float
FAB_FLOATING(float, float, "%.9g")
extern "C" double      fab_to_double_float(float x)  { return (double)x; }
extern "C" long double fab_to_ldouble_float(float x) { return (long double)x; }

// Double
FAB_FLOATING(double, double, "%.17g")
extern "C" float       fab_to_float_double(double x)   { return (float)x; }
extern "C" long double fab_to_ldouble_double(double x)  { return (long double)x; }

// Long double (x87 80 bit, 64-bit mantissa)
// Saturation is exact here: LLONG_MIN / LLONG_MAX fit into the 64-bit mantissa without rounding
static long long fab_sat_ld(long double x, long long lo, long long hi)
{
    if (x != x) return 0;
    if (x <= (long double)lo) return lo;
    if (x >= (long double)hi) return hi;
    return (long long)x;
}

extern "C" char* fab_to_str_ldouble(long double x)         { return fab_fmt_alloc_ld("%.21Lg", x); }
extern "C" bool  fab_is_positive_ldouble(long double x)    { return x > 0; }
extern "C" bool  fab_is_negative_ldouble(long double x)    { return x < 0; }
extern "C" bool  fab_is_zero_ldouble(long double x)        { return x == 0; }
extern "C" bool  fab_is_nan_ldouble(long double x)         { return x != x; }
extern "C" bool  fab_is_inf_ldouble(long double x)         { return x == x && (x - x) != 0; }
extern "C" bool  fab_is_finite_ldouble(long double x)      { return (x - x) == 0; }
extern "C" long double fab_abs_ldouble(long double x)      { return x < 0 ? -x : x; }
extern "C" int   fab_sign_ldouble(long double x)           { return (x > 0) - (x < 0); }
extern "C" long double fab_floor_ldouble(long double x)    { return floorl(x); }
extern "C" long double fab_ceil_ldouble(long double x)     { return ceill(x); }
extern "C" long double fab_trunc_ldouble(long double x)    { return x < 0 ? ceill(x) : floorl(x); }

// Round half away from zero (a + 0.5 would lose precision near 0.5, so compare the fraction instead)
extern "C" long double fab_round_ldouble(long double x)
{
    if (x != x || (x - x) != 0) return x;      // NaN, inf
    long double a = x < 0 ? -x : x;
    long double r = floorl(a);
    if (a - r >= 0.5L) r += 1.0L;
    return x < 0 ? -r : r;
}

extern "C" bool  fab_to_bool_ldouble(long double x)        { return x != 0; }
extern "C" char  fab_to_char_ldouble(long double x)        { return (char)fab_sat_ld(x, -128LL, 127LL); }
extern "C" short fab_to_short_ldouble(long double x)       { return (short)fab_sat_ld(x, -32768LL, 32767LL); }
extern "C" int   fab_to_int_ldouble(long double x)         { return (int)fab_sat_ld(x, -2147483648LL, 2147483647LL); }
extern "C" long long fab_to_long_ldouble(long double x)    { return fab_sat_ld(x, (-9223372036854775807LL - 1), 9223372036854775807LL); }
extern "C" float  fab_to_float_ldouble(long double x)      { return (float)x; }
extern "C" double fab_to_double_ldouble(long double x)     { return (double)x; }