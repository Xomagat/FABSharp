//
// Created by Xomagat on 15.11.2026.
//

#include <stddef.h>
#include "../alloc.h"

typedef unsigned int       u32;
typedef unsigned long long u64;

// ---------- generic helpers (U = unsigned type of the same width) ----------
template<class U> static int popcnt(U x)
{
    const U m1 = (U)~(U)0 / 3, m2 = (U)~(U)0 / 15 * 3,
            m4 = (U)~(U)0 / 255 * 15, h = (U)~(U)0 / 255;
    x = x - ((x >> 1) & m1);
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    return (int)((x * h) >> (sizeof(U) * 8 - 8));
}

template<class U> static int clz_(U x)
{
    const int W = (int)(sizeof(U) * 8);
    if (!x) return W;
    int n = 0;
    for (int s = W / 2; s; s >>= 1)
        if (!(x >> (W - s))) { n += s; x <<= s; }
    return n;
}

template<class U> static int ctz_(U x) { return popcnt((U)(~x & (U)(x - 1))); }   // x == 0 -> W

template<class U> static U shl_(U x, int n)
{
    return (n < 0 || n >= (int)(sizeof(U) * 8)) ? (U)0 : (U)(x << n);
}

template<class U> static U ushr_(U x, int n)
{
    return (n < 0 || n >= (int)(sizeof(U) * 8)) ? (U)0 : (U)(x >> n);
}

// Arithmetic shift without relying on >> of a negative number
template<class S, class U> static S sar_(S x, int n)
{
    const int W = (int)(sizeof(U) * 8);
    if (n < 0 || n >= W) n = W - 1;
    U u = (U)x;
    return (S)(x < 0 ? (U)~((U)~u >> n) : (U)(u >> n));
}

template<class U> static U rotl_(U x, int n)
{
    const int W = (int)(sizeof(U) * 8);
    int k = n % W; if (k < 0) k += W;
    return k == 0 ? x : (U)((x << k) | (x >> (W - k)));
}

template<class U> static U bswap_(U x)
{
    U r = 0;
    for (size_t i = 0; i < sizeof(U); ++i) { r = (U)((r << 8) | (x & 0xFF)); x >>= 8; }
    return r;
}

template<class U> static U rev_(U x)
{
    U r = 0;
    for (size_t i = 0; i < sizeof(U) * 8; ++i) { r = (U)((r << 1) | (x & 1)); x >>= 1; }
    return r;
}

template<class U> static U mask_(int n)
{
    const int W = (int)(sizeof(U) * 8);
    if (n <= 0) return 0;
    if (n >= W) return (U)~(U)0;
    return (U)(((U)1 << n) - 1);
}

// Digits in base 2^shift, lowercase. width = minimum length (0 = no padding).
template<class U> static char* to_base(U x, int shift, int width)
{
    const char* D = "0123456789abcdef";
    const U mask = (U)((1 << shift) - 1);
    char tmp[64];
    int n = 0;
    do { tmp[n++] = D[(int)(x & mask)]; x >>= shift; } while (x);

    if (width < 0) width = 0;
    if (width > 1024) width = 1024;
    int len = n > width ? n : width;

    char* r = (char*)bump_alloc((size_t)len + 1);
    int pad = len - n;
    for (int i = 0; i < pad; ++i) r[i] = '0';
    for (int i = 0; i < n; ++i)   r[pad + i] = tmp[n - 1 - i];
    r[len] = '\0';
    return r;
}

// ---------- exports: SFX = i / l, T = int / long long, UT = its unsigned type ----------
#define FAB_BITS(SFX, T, UT) \
  extern "C" T    fab_band##SFX(T a, T b)       { return (T)((UT)a & (UT)b); } \
  extern "C" T    fab_bor##SFX(T a, T b)        { return (T)((UT)a | (UT)b); } \
  extern "C" T    fab_bxor##SFX(T a, T b)       { return (T)((UT)a ^ (UT)b); } \
  extern "C" T    fab_bnot##SFX(T a)            { return (T)~(UT)a; } \
  extern "C" T    fab_shl##SFX(T a, int n)      { return (T)shl_<UT>((UT)a, n); } \
  extern "C" T    fab_shr##SFX(T a, int n)      { return sar_<T, UT>(a, n); } \
  extern "C" T    fab_ushr##SFX(T a, int n)     { return (T)ushr_<UT>((UT)a, n); } \
  extern "C" T    fab_rotl##SFX(T a, int n)     { return (T)rotl_<UT>((UT)a, n); } \
  extern "C" T    fab_rotr##SFX(T a, int n)     { return (T)rotl_<UT>((UT)a, (int)(-(long long)n)); } \
  extern "C" int  fab_popcount##SFX(T a)        { return popcnt<UT>((UT)a); } \
  extern "C" int  fab_clz##SFX(T a)             { return clz_<UT>((UT)a); } \
  extern "C" int  fab_ctz##SFX(T a)             { return ctz_<UT>((UT)a); } \
  extern "C" int  fab_bit_length##SFX(T a)      { return (int)(sizeof(UT) * 8) - clz_<UT>((UT)a); } \
  extern "C" bool fab_parity##SFX(T a)          { return (popcnt<UT>((UT)a) & 1) != 0; } \
  extern "C" bool fab_bit_get##SFX(T a, int i) \
      { return i >= 0 && i < (int)(sizeof(UT) * 8) && (((UT)a >> i) & 1); } \
  extern "C" T    fab_bit_set##SFX(T a, int i) \
      { return (i < 0 || i >= (int)(sizeof(UT) * 8)) ? a : (T)((UT)a | ((UT)1 << i)); } \
  extern "C" T    fab_bit_clear##SFX(T a, int i) \
      { return (i < 0 || i >= (int)(sizeof(UT) * 8)) ? a : (T)((UT)a & ~((UT)1 << i)); } \
  extern "C" T    fab_bit_toggle##SFX(T a, int i) \
      { return (i < 0 || i >= (int)(sizeof(UT) * 8)) ? a : (T)((UT)a ^ ((UT)1 << i)); } \
  extern "C" T    fab_bit_mask##SFX(int n)      { return (T)mask_<UT>(n); } \
  extern "C" T    fab_bit_extract##SFX(T a, int pos, int len) \
      { return (T)(ushr_<UT>((UT)a, pos) & mask_<UT>(len)); } \
  extern "C" T    fab_bswap##SFX(T a)           { return (T)bswap_<UT>((UT)a); } \
  extern "C" T    fab_bit_reverse##SFX(T a)     { return (T)rev_<UT>((UT)a); } \
  extern "C" char* fab_bin_str##SFX(T a, int width) { return to_base<UT>((UT)a, 1, width); } \
  extern "C" char* fab_hex_str##SFX(T a, int width) { return to_base<UT>((UT)a, 4, width); }

FAB_BITS(i, int,       u32)
FAB_BITS(l, long long, u64)