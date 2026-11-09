//
// Created by Xomagat on 05.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../alloc.h"
#define malloc(n) bump_alloc(n)

static char g_empty[1] = {0};
static inline char* empty_str() { return g_empty; }

static inline const char* S(const char* x) { return x ? x : ""; }

extern "C" void* fab_list_new_string();
extern "C" void  fab_list_addend_string(void* l, const char* v);

static char* str_dup_n(const char* s, size_t n)
{
    char* r = (char*)malloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

extern "C" int  fab_length(const char* x) { return static_cast<int>(strlen(S(x))); }
extern "C" bool fab_empty(const char* x)  { return !x || *x == '\0'; }

extern "C" int fab_findc(const char* x, const char y)
{
    if (y == '\0') return -1;
    x = S(x);
    const char* p = strchr(x, y);
    return p ? (int)(p - x) : -1;
}

extern "C" int fab_finds(const char* x, const char* y)
{
    x = S(x);
    const char* p = strstr(x, S(y));
    return p ? (int)(p - x) : -1;
}

extern "C" bool fab_contains(const char* x, const char* y) { return strstr(S(x), S(y)) != nullptr; }
extern "C" int  fab_compare(const char* x, const char* y)  { return strcmp(S(x), S(y)); }
extern "C" char fab_first(const char* x)                   { return S(x)[0]; }

extern "C" char fab_last(const char* x)
{
    x = S(x);
    size_t n = strlen(x);
    return n > 0 ? x[n - 1] : '\0';
}

extern "C" char fab_at(const char* x, int i)
{
    if (!x || i < 0) return '\0';
    for (int k = 0; k < i; ++k)
        if (!x[k]) return '\0';
    return x[i];
}

extern "C" int fab_count(const char* x, const char* y)
{
    x = S(x); y = S(y);
    if (*y == '\0') return 0;
    int c = 0;
    size_t y_len = strlen(y);
    for (const char* p = x; (p = strstr(p, y)); p += y_len) c++;
    return c;
}

extern "C" bool fab_is_blank(const char* x)
{
    for (x = S(x); *x; x++)
        if (!isspace((unsigned char)*x)) return false;
    return true;
}

extern "C" char* fab_substr(const char* src, int start, int length)
{
    src = S(src);
    int src_len = (int)strlen(src);
    if (start < 0 || length <= 0 || start >= src_len) return empty_str();

    if (length > src_len - start) length = src_len - start;
    return str_dup_n(src + start, (size_t)length);
}

extern "C" char* fab_to_upper(const char* x)
{
    x = S(x);
    size_t len = strlen(x);
    char* r = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; i++) r[i] = (char)toupper((unsigned char)x[i]);
    r[len] = '\0';
    return r;
}

extern "C" char* fab_to_lower(const char* x)
{
    x = S(x);
    size_t len = strlen(x);
    char* r = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; i++) r[i] = (char)tolower((unsigned char)x[i]);
    r[len] = '\0';
    return r;
}

extern "C" char* fab_replace(const char* x, const char* y, const char* z)
{
    x = S(x); y = S(y); z = S(z);
    size_t y_len = strlen(y);
    if (y_len == 0) return (char*)x;

    size_t z_len = strlen(z), x_len = strlen(x), count = 0;
    for (const char* t = x; (t = strstr(t, y)); t += y_len) count++;
    if (count == 0) return (char*)x;

    char* result = (char*)malloc(x_len - count * y_len + count * z_len + 1);
    char* o = result;
    const char* p;
    while ((p = strstr(x, y)))
    {
        memcpy(o, x, (size_t)(p - x)); o += p - x;
        memcpy(o, z, z_len);           o += z_len;
        x = p + y_len;
    }
    memcpy(o, x, strlen(x) + 1);
    return result;
}

extern "C" char* fab_trim(const char* x)
{
    x = S(x);
    while (isspace((unsigned char)*x)) x++;
    size_t len = strlen(x);
    while (len > 0 && isspace((unsigned char)x[len - 1])) len--;
    if (x[len] == '\0') return (char*)x;
    return str_dup_n(x, len);
}

extern "C" char* fab_repeat(const char* x, int n)
{
    x = S(x);
    size_t len = strlen(x);
    if (n <= 0 || len == 0) return empty_str();
    if (n == 1) return (char*)x;
    if (len > ((size_t)-1 - 1) / (size_t)n) fab_oom();

    size_t total = len * (size_t)n;
    char* r = (char*)malloc(total + 1);
    for (int i = 0; i < n; i++) memcpy(r + len * (size_t)i, x, len);
    r[total] = '\0';
    return r;
}

extern "C" char* fab_reverse(const char* x)
{
    x = S(x);
    size_t len = strlen(x);
    char* r = str_dup_n(x, len);
    for (size_t i = 0; i < len / 2; i++)
    {
        char t = r[i]; r[i] = r[len - 1 - i]; r[len - 1 - i] = t;
    }
    return r;
}

extern "C" char* fab_capitalize(const char* x)
{
    x = S(x);
    if (*x == '\0') return (char*)x;
    char* r = str_dup_n(x, strlen(x));
    r[0] = (char)toupper((unsigned char)r[0]);
    return r;
}

extern "C" char* fab_pad_left(const char* x, int width, char fill)
{
    x = S(x);
    size_t len = strlen(x);
    if (width <= 0 || (size_t)width <= len) return (char*)x;

    char* r = (char*)malloc((size_t)width + 1);
    size_t pad = (size_t)width - len;
    memset(r, fill, pad);
    memcpy(r + pad, x, len + 1);
    return r;
}

extern "C" char* fab_pad_right(const char* x, int width, char fill)
{
    x = S(x);
    size_t len = strlen(x);
    if (width <= 0 || (size_t)width <= len) return (char*)x;

    char* r = (char*)malloc((size_t)width + 1);
    size_t pad = (size_t)width - len;
    memcpy(r, x, len);
    memset(r + len, fill, pad);
    r[width] = '\0';
    return r;
}

extern "C" bool fab_start_with(const char* x, const char* y)
{
    if (!x || !y) return false;

    size_t len_prefix = strlen(y);
    size_t len_str = strlen(x);

    if (len_prefix > len_str) {
        return false;
    }

    return strncmp(x, y, len_prefix) == 0;
}

extern "C" bool fab_ends_with(const char* x, const char* y)
{
    if (!x || !y) return false;

    size_t str_len = strlen(x);
    size_t suffix_len = strlen(y);

    if (suffix_len > str_len) return false;

    return strcmp(x + str_len - suffix_len, y) == 0;
}

extern "C" int fab_to_int(const char* x)
{
    char *p = nullptr;
    return strtol(S(x), &p, 10);
}

extern "C" long long fab_to_long(const char* x)
{
    char *p = nullptr;
    return strtoll(S(x), &p, 10);
}

extern "C" double fab_to_double(const char* x)
{
    char *p = nullptr;
    return strtod(S(x), &p);
}

extern "C" bool fab_to_bool(const char* x)
{
    return x != nullptr && strcmp(x, "true") == 0;
}

extern "C" bool fab_is_numeric(const char* x)
{
    if (x == nullptr || *x == '\0') return false;

    for (; *x; x++)
        if (!isdigit(static_cast<unsigned char>(*S(x)))) return false;
    return true;
}

extern "C" int fab_rfinds(const char* x, const char* y)
{
    x = S(x); y = S(y);
    size_t xl = strlen(x), yl = strlen(y);
    if (yl == 0) return (int)xl;
    if (yl > xl) return -1;

    for (const char* p = x + (xl - yl); ; --p)
    {
        if (memcmp(p, y, yl) == 0) return (int)(p - x);
        if (p == x) return -1;
    }
}

extern "C" bool fab_equals_ignore_case(const char* x, const char* y)
{
    if (x == nullptr || y == nullptr) return false;

    for (; *x && *y; x++, y++)
        if (tolower(static_cast<unsigned char>(*x)) != tolower(static_cast<unsigned char>(*y)))
            return false;

    return *x == *y;
}

extern "C" void* fab_split(const char* x, const char* sep)
{
    void* list = fab_list_new_string();
    if (x == nullptr) return list;

    if (sep == nullptr || *sep == '\0')
    {
        fab_list_addend_string(list, str_dup_n(x, strlen(x)));
        return list;
    }

    size_t sep_len = strlen(sep);
    const char* start = x;

    while (const char* p = strstr(start, sep))
    {
        fab_list_addend_string(list, str_dup_n(start, (size_t)(p - start)));
        start = p + sep_len;
    }
    fab_list_addend_string(list, str_dup_n(start, strlen(start)));

    return list;
}