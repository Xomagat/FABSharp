//
// Created by Xomagat on 08.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>

// Int
extern "C" char* fab_to_str_int(const int* x)
{
    int size = snprintf(nullptr, 0, "%d", x) + 1;

    char *p = new char[size];

    snprintf(p, size, "%d", x);

    return p;
}

extern "C" bool fab_is_positive_int(int x)
{
    return x > 0;
}

extern "C" bool fab_is_negative_int(int x)
{
    return x < 0;
}

// Short
extern "C" char* fab_to_str_short(const short* x)
{
    int size = snprintf(nullptr, 0, "%d", x) + 1;

    char *p = new char[size];

    snprintf(p, size, "%d", x);

    return p;
}

extern "C" bool fab_is_positive_short(short x)
{
    return x > 0;
}

extern "C" bool fab_is_negative_short(short x)
{
    return x < 0;
}

// Long
extern "C" char* fab_to_str_long(const long long* x)
{
    int size = snprintf(nullptr, 0, "%d", x) + 1;

    char *p = new char[size];

    snprintf(p, size, "%d", x);

    return p;
}

extern "C" bool fab_is_positive_long(long long x)
{
    return x > 0;
}

extern "C" bool fab_is_negative_long(long long x)
{
    return x < 0;
}

// Float
extern "C" char* fab_to_str_float(const float* x)
{
    int size = snprintf(nullptr, 0, "%d", x) + 1;

    char *p = new char[size];

    snprintf(p, size, "%p", x);

    return p;
}

extern "C" bool fab_is_positive_float(float x)
{
    return x > 0;
}

extern "C" bool fab_is_negative_float(char x)
{
    return x < 0;
}

// Double
extern "C" char* fab_to_str_double(const double* x)
{
    int size = snprintf(nullptr, 0, "%d", x) + 1;

    char *p = new char[size];

    snprintf(p, size, "%p", x);

    return p;
}

extern "C" bool fab_is_positive_double(double x)
{
    return x > 0;
}

extern "C" bool fab_is_negative_double(double x)
{
    return x < 0;
}