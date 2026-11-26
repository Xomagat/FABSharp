//
// Created by Xomagat on 24.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "../alloc.h"

static inline const char* S(const char* x) { return x ? x : ""; }

// Result strings live in the bump allocator (never freed, like everywhere in the lib)
static char* make_str(const char* s, size_t n)
{
    char* r = (char*)bump_alloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}
static char* empty_str() { return make_str("", 0); }

static const char* DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

// '0'-'9', 'a'-'z', 'A'-'Z' -> 0..35, anything else -> -1
static int digit_of(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

static bool ieq(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    return *a == *b;
}

// Strict parser of the magnitude.
// Accepts: optional sign, optional prefix that matches the base (0x / 0b / 0o), digits.
// Rejects: empty text, spaces, '_', digits >= base, overflow of unsigned 64 bit.
static bool parse_mag(const char* s, int base, bool* neg, unsigned long long* mag)
{
    s = S(s);
    if (base < 2 || base > 36) return false;

    *neg = false;
    if (*s == '-' || *s == '+') { *neg = (*s == '-'); ++s; }

    if (s[0] == '0' && s[1])
    {
        char p = (char)tolower((unsigned char)s[1]);
        if ((base == 16 && p == 'x') || (base == 2 && p == 'b') || (base == 8 && p == 'o')) s += 2;
    }
    if (!*s) return false;

    unsigned long long v = 0;
    for (; *s; ++s)
    {
        int d = digit_of(*s);
        if (d < 0 || d >= base) return false;
        if (v > (ULLONG_MAX - (unsigned)d) / (unsigned)base) return false;   // v*base + d would overflow
        v = v * (unsigned)base + (unsigned)d;
    }
    *mag = v;
    return true;
}

// Signed 64 bit on top of it
static bool parse_i64(const char* s, int base, long long* out)
{
    bool neg; unsigned long long m;
    if (!parse_mag(s, base, &neg, &m)) return false;

    if (neg) { if (m > 9223372036854775808ULL) return false; *out = (long long)(0ULL - m); }
    else     { if (m > 9223372036854775807ULL) return false; *out = (long long)m; }
    return true;
}

// Digits of a magnitude in any base 2..36 (the caller checked the base)
static char* fmt_base(unsigned long long mag, bool neg, int base)
{
    char buf[72];                                  // 64 digits (base 2) + sign + '\0'
    char* p = buf + sizeof buf;
    *--p = '\0';
    do { *--p = DIGITS[mag % (unsigned)base]; mag /= (unsigned)base; } while (mag);
    if (neg) *--p = '-';
    return make_str(p, (size_t)(buf + sizeof buf - 1 - p));
}

// convert.to_str(long n, int base=10)
extern "C" char* fab_convert_to_base(long long n, int base)
{
    if (base < 2 || base > 36) return empty_str();
    bool neg = n < 0;
    unsigned long long mag = neg ? 0ULL - (unsigned long long)n : (unsigned long long)n;
    return fmt_base(mag, neg, base);
}

// ---------- checks and safe parsing of numbers ----------
extern "C" bool fab_convert_is_int(const char* s, int base)
{
    long long v;
    return parse_i64(s, base, &v) && v >= INT_MIN && v <= INT_MAX;
}
extern "C" bool fab_convert_is_long(const char* s, int base)
{
    long long v;
    return parse_i64(s, base, &v);
}

// convert.to_int(text, base=10, fallback=0)
extern "C" int fab_convert_parse_int(const char* s, int base, int fallback)
{
    long long v;
    return (parse_i64(s, base, &v) && v >= INT_MIN && v <= INT_MAX) ? (int)v : fallback;
}
// convert.to_long(text, base=10, fallback=0)
extern "C" long long fab_convert_parse_long(const char* s, int base, long long fallback)
{
    long long v;
    return parse_i64(s, base, &v) ? v : fallback;
}

// Double: only digits, sign, '.', 'e'. This rejects "inf", "nan", hex floats and spaces,
// which strtod would happily accept.
static bool parse_dbl(const char* s, double* out)
{
    s = S(s);
    if (!*s) return false;
    for (const char* p = s; *p; ++p)
        if (!((*p >= '0' && *p <= '9') || *p == '+' || *p == '-' || *p == '.' || *p == 'e' || *p == 'E'))
            return false;

    char* end = nullptr;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return false;
    *out = v;
    return true;
}
extern "C" bool   fab_convert_is_double(const char* s) { double v; return parse_dbl(s, &v); }
extern "C" double fab_convert_parse_double(const char* s, double fallback)
{
    double v;
    return parse_dbl(s, &v) ? v : fallback;
}

// true: true 1 yes on y     false: false 0 no off n      (any case). Anything else: -1
static int bool_of(const char* s)
{
    s = S(s);
    static const char* T[] = {"true", "1", "yes", "on", "y"};
    static const char* F[] = {"false", "0", "no", "off", "n"};
    for (const char* t : T) if (ieq(s, t)) return 1;
    for (const char* f : F) if (ieq(s, f)) return 0;
    return -1;
}

// Bool arrives from the compiler as i1: the upper bits are not defined, so mask with & 1
extern "C" bool fab_convert_is_bool(const char* s) { return bool_of(s) >= 0; }
extern "C" bool fab_convert_parse_bool(const char* s, unsigned char fallback)
{
    int b = bool_of(s);
    return b >= 0 ? b == 1 : (fallback & 1) != 0;
}

// "AB" -> "4142"
extern "C" char* fab_convert_bytes_to_hex(const char* s)
{
    s = S(s);
    size_t n = strlen(s);
    static const char* H = "0123456789abcdef";
    char* r = (char*)bump_alloc(n * 2 + 1);
    for (size_t i = 0; i < n; ++i)
    {
        unsigned char c = (unsigned char)s[i];
        r[2 * i]     = H[c >> 4];
        r[2 * i + 1] = H[c & 15];
    }
    r[2 * n] = '\0';
    return r;
}

// "4142" -> "AB". Odd length, a non-hex digit or a 00 byte -> "" (a string cannot contain '\0')
extern "C" char* fab_convert_hex_to_bytes(const char* s)
{
    s = S(s);
    size_t n = strlen(s);
    if (n % 2) return empty_str();

    char* r = (char*)bump_alloc(n / 2 + 1);
    for (size_t i = 0; i < n / 2; ++i)
    {
        int a = digit_of(s[2 * i]), b = digit_of(s[2 * i + 1]);
        if (a < 0 || a > 15 || b < 0 || b > 15 || (a == 0 && b == 0)) return empty_str();
        r[i] = (char)(a * 16 + b);
    }
    r[n / 2] = '\0';
    return r;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

extern "C" char* fab_convert_to_base64(const char* s)
{
    s = S(s);
    size_t n = strlen(s);
    char* r = (char*)bump_alloc((n + 2) / 3 * 4 + 1);
    char* p = r;
    for (size_t i = 0; i < n; i += 3)
    {
        unsigned v = (unsigned)(unsigned char)s[i] << 16;
        if (i + 1 < n) v |= (unsigned)(unsigned char)s[i + 1] << 8;
        if (i + 2 < n) v |= (unsigned)(unsigned char)s[i + 2];
        *p++ = B64[(v >> 18) & 63];
        *p++ = B64[(v >> 12) & 63];
        *p++ = i + 1 < n ? B64[(v >> 6) & 63] : '=';
        *p++ = i + 2 < n ? B64[v & 63]        : '=';
    }
    *p = '\0';
    return r;
}

static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Invalid text (length not a multiple of 4, bad characters) -> ""
extern "C" char* fab_convert_from_base64(const char* s)
{
    s = S(s);
    size_t n = strlen(s);
    if (n == 0 || n % 4) return empty_str();

    size_t pad = 0;
    if (s[n - 1] == '=') { pad = 1; if (s[n - 2] == '=') pad = 2; }

    char* r = (char*)bump_alloc(n / 4 * 3 + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i += 4)
    {
        bool last = (i + 4 == n);
        int a = b64_val(s[i]), b = b64_val(s[i + 1]);
        int c = (last && pad == 2) ? 0 : b64_val(s[i + 2]);
        int d = (last && pad >= 1) ? 0 : b64_val(s[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return empty_str();

        unsigned v = ((unsigned)a << 18) | ((unsigned)b << 12) | ((unsigned)c << 6) | (unsigned)d;
        char bytes[3] = {(char)(v >> 16), (char)(v >> 8), (char)v};
        size_t cnt = last ? 3 - pad : 3;
        for (size_t k = 0; k < cnt; ++k)
        {
            if (bytes[k] == '\0') return empty_str();
            r[o++] = bytes[k];
        }
    }
    r[o] = '\0';
    return r;
}

extern "C" char* fab_convert_to_roman(int n)
{
    if (n < 1 || n > 3999) return empty_str();
    static const int   V[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const char* R[] = {"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};
    char buf[32];                                  // longest: 3888 -> 15 chars
    size_t len = 0;
    for (int i = 0; i < 13; ++i)
        while (n >= V[i])
        {
            size_t l = strlen(R[i]);
            memcpy(buf + len, R[i], l);
            len += l;
            n -= V[i];
        }
    return make_str(buf, len);
}

static int roman_val(char c)
{
    switch (tolower((unsigned char)c))
    {
        case 'i': return 1;   case 'v': return 5;   case 'x': return 10;
        case 'l': return 50;  case 'c': return 100; case 'd': return 500;
        case 'm': return 1000;
        default:  return 0;
    }
}

// Invalid or non-canonical text ("IIII", "VX", "IC") -> 0
extern "C" int fab_convert_from_roman(const char* s)
{
    s = S(s);
    if (!*s) return 0;

    int total = 0;
    for (size_t i = 0; s[i]; ++i)
    {
        int a = roman_val(s[i]);
        if (!a) return 0;
        int b = s[i + 1] ? roman_val(s[i + 1]) : 0;
        total += (a < b) ? -a : a;                 // a smaller digit before a bigger one is subtracted
    }
    if (total < 1 || total > 3999) return 0;

    // Round trip: only the canonical spelling is accepted
    return ieq(s, fab_convert_to_roman(total)) ? total : 0;
}

// 1536 -> "1.50 KB" (steps of 1024)
extern "C" char* fab_convert_size_str(long long bytes)
{
    static const char* U[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    bool neg = bytes < 0;
    double v = neg ? -(double)bytes : (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 6) { v /= 1024.0; ++u; }

    char buf[48];
    int n = (u == 0)
        ? snprintf(buf, sizeof buf, "%s%lld B", neg ? "-" : "", (long long)v)
        : snprintf(buf, sizeof buf, "%s%.2f %s", neg ? "-" : "", v, U[u]);
    if (n < 0) n = 0;
    if (n >= (int)sizeof buf) n = (int)sizeof buf - 1;
    return make_str(buf, (size_t)n);
}

// ============ hex / oct / bin ============
// One struct for all three classes: the value (64 bit) and the radix (16, 8 or 2).
// The radix only decides how the number is printed and parsed by default.
// The struct is malloc'ed (freed in fab_num_free), text results go to the bump allocator.
struct FabNum { unsigned long long v; int radix; };

static inline unsigned long long nv(void* h) { return h ? ((FabNum*)h)->v : 0; }
static inline int nr(void* h)                { return h ? ((FabNum*)h)->radix : 16; }
static inline int shift_of(int radix)        { return radix == 16 ? 4 : radix == 8 ? 3 : 1; }

static FabNum* num_make(unsigned long long v, int radix)
{
    auto* h = (FabNum*)fab_xmalloc(sizeof(FabNum));
    h->v = v;
    h->radix = radix;
    return h;
}

// Text -> unsigned value. Fails on bad text, overflow and on a minus sign ("-0" is fine)
static bool parse_unsigned(const char* s, int base, unsigned long long* out)
{
    bool neg; unsigned long long m;
    if (!parse_mag(s, base, &neg, &m) || (neg && m != 0)) return false;
    *out = m;
    return true;
}

// Bad text -> null (all methods below are null-safe, so a null object acts like 0)
static void* num_parse(const char* s, int base, int radix)
{
    unsigned long long m;
    return parse_unsigned(s, base, &m) ? num_make(m, radix) : nullptr;
}

extern "C" void  fab_num_free(void* h) { free(h); }                     // free(nullptr) is safe
extern "C" void* fab_num_copy(void* h) { return h ? num_make(nv(h), nr(h)) : nullptr; }

// fab_<name>_zero / _parse / _of / _from_num       -> hex(), hex("ff"), hex(255), hex(other)
// fab_convert_to_<name>(text, from)                  -> convert.to_hex("1010", 2)
#define FAB_NUM_CLASS(NAME, RADIX) \
    extern "C" void* fab_##NAME##_zero()                { return num_make(0, RADIX); } \
    extern "C" void* fab_##NAME##_parse(const char* s)  { return num_parse(s, RADIX, RADIX); } \
    extern "C" void* fab_##NAME##_of(long long n)       { return num_make((unsigned long long)n, RADIX); } \
    extern "C" void* fab_##NAME##_from_num(void* o)     { return o ? num_make(nv(o), RADIX) : nullptr; } \
    extern "C" void* fab_convert_to_##NAME(const char* s, int from) { return num_parse(s, from, RADIX); }

FAB_NUM_CLASS(hex, 16)
FAB_NUM_CLASS(oct, 8)
FAB_NUM_CLASS(bin, 2)

static char* num_format(void* h, int width, bool prefix, bool upper)
{
    unsigned long long v = nv(h);
    int radix = nr(h), sh = shift_of(radix);
    const char* D = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    char tmp[64];                                        // 64 binary digits is the maximum
    int n = 0;
    do { tmp[n++] = D[v & (unsigned)(radix - 1)]; v >>= sh; } while (v);

    if (width < 0) width = 0;
    if (width > 1024) width = 1024;
    int digits = n > width ? n : width;
    size_t pre = prefix ? 2 : 0;

    char* r = (char*)bump_alloc(pre + (size_t)digits + 1);
    char* p = r;
    if (pre) { *p++ = '0'; *p++ = radix == 16 ? 'x' : radix == 8 ? 'o' : 'b'; }
    for (int i = n; i < digits; ++i) *p++ = '0';        // leading zeros up to width
    while (n) *p++ = tmp[--n];                           // digits were collected backwards
    *p = '\0';
    return r;
}

// Bool arrives from the compiler as i1: the upper bits are not defined, so mask with & 1
extern "C" char* fab_num_format(void* h, int width, unsigned char prefix, unsigned char upper)
{
    return num_format(h, width, (prefix & 1) != 0, (upper & 1) != 0);
}

extern "C" char* fab_num_to_str(void* h)  { return num_format(h, 0, false, false); }
extern "C" char* fab_num_to_dec(void* h)  { return fmt_base(nv(h), false, 10); }   // unsigned decimal
extern "C" char* fab_num_to_base(void* h, int base)
{
    return (base < 2 || base > 36) ? empty_str() : fmt_base(nv(h), false, base);
}

extern "C" int       fab_num_to_int(void* h)    { return (int)(unsigned)nv(h); }   // low 32 bits
extern "C" long long fab_num_to_long(void* h)   { return (long long)nv(h); }
extern "C" double    fab_num_to_double(void* h) { return (double)nv(h); }          // exact up to 2^53
extern "C" bool      fab_num_to_bool(void* h)   { return nv(h) != 0; }

// Text -> value. On any error returns false and the old value stays
static bool num_set_text(void* h, const char* s, int base)
{
    unsigned long long m;
    if (!h || !parse_unsigned(s, base, &m)) return false;
    ((FabNum*)h)->v = m;
    return true;
}

extern "C" bool fab_num_from_str(void* h, const char* s)  { return h && num_set_text(h, s, nr(h)); }
extern "C" bool fab_num_from_hex(void* h, const char* s)  { return num_set_text(h, s, 16); }
extern "C" bool fab_num_from_oct(void* h, const char* s)  { return num_set_text(h, s, 8); }
extern "C" bool fab_num_from_bin(void* h, const char* s)  { return num_set_text(h, s, 2); }
extern "C" bool fab_num_from_dec(void* h, const char* s)  { return num_set_text(h, s, 10); }
extern "C" bool fab_num_from_base(void* h, const char* s, int base) { return num_set_text(h, s, base); }

// A negative number keeps its 64-bit pattern (sign is extended)
extern "C" void fab_num_from_long(void* h, long long n) { if (h) ((FabNum*)h)->v = (unsigned long long)n; }
extern "C" void fab_num_from_int(void* h, int n)        { fab_num_from_long(h, n); }

extern "C" int fab_num_base(void* h) { return nr(h); }

extern "C" int fab_num_length(void* h)                   // digits in the own radix
{
    unsigned long long v = nv(h);
    int sh = shift_of(nr(h)), n = 0;
    do { ++n; v >>= sh; } while (v);
    return n;
}
extern "C" int fab_num_bit_length(void* h)
{
    unsigned long long v = nv(h); int n = 0;
    while (v) { ++n; v >>= 1; }
    return n;
}
extern "C" int fab_num_popcount(void* h)
{
    unsigned long long v = nv(h); int n = 0;
    while (v) { v &= v - 1; ++n; }                       // drops the lowest set bit
    return n;
}
extern "C" bool fab_num_is_zero(void* h) { return nv(h) == 0; }
extern "C" bool fab_num_is_even(void* h) { return (nv(h) & 1) == 0; }
extern "C" bool fab_num_is_odd(void* h)  { return (nv(h) & 1) != 0; }

// Null objects: equals is false, compare treats null as 0 (same rule as in date)
extern "C" bool fab_num_equals(void* a, void* b) { return a && b && nv(a) == nv(b); }
extern "C" int  fab_num_compare(void* a, void* b)
{
    unsigned long long x = nv(a), y = nv(b);
    return (x > y) - (x < y);
}

// convert.is_hex / is_oct / is_bin: the text must be valid for hex("...") etc.
extern "C" bool fab_convert_is_hex(const char* s) { unsigned long long m; return parse_unsigned(s, 16, &m); }
extern "C" bool fab_convert_is_oct(const char* s) { unsigned long long m; return parse_unsigned(s, 8, &m); }
extern "C" bool fab_convert_is_bin(const char* s) { unsigned long long m; return parse_unsigned(s, 2, &m); }

