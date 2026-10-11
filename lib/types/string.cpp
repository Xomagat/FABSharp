//
// Created by Xomagat on 05.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern "C" void* fab_list_new_string();
extern "C" void  fab_list_addend_string(void* l, const char* v);

static char* str_dup_n(const char* s, size_t n)
{
    char* r = (char*)malloc(n + 1);
    if (r == nullptr) return nullptr;

    memcpy(r, s, n);
    r[n] = '\0';

    return r;
}

extern "C" int fab_length(const char* x)
{
    return strlen(x);
}

extern "C" bool fab_empty(const char* x)
{
    return strlen(x) == 0;
}

extern "C" char* fab_clear(char* x)
{
    *x = '\0';
    return x;
}

extern "C" int fab_findc(const char* x, const char y)
{
    char const *p = strchr(x, y);
    return p - x;
}

extern "C" int fab_finds(const char* x, const char* y)
{
    const char* p = strstr(x, y);
    return p ? (int)(p - x) : -1;
}

extern "C" bool fab_contains(const char* x, const char* y)
{
    return strstr(x, y) != nullptr;
}

extern "C" char* fab_substr(const char *src, int start, int length)
{
    if (src == NULL || start < 0 || length <= 0) return NULL;

    int src_len = strlen(src);
    if (start >= src_len) return NULL;

    if (start + length > src_len) {
        length = src_len - start;
    }

    char* dest = (char*)malloc((length + 1) * sizeof(char));
    if (dest == NULL) return NULL;

    strncpy(dest, src + start, length);
    dest[length] = '\0';

    return dest;
}

extern "C" int fab_compare(const char* x, const char* y)
{
    return strcmp(x, y);
}

extern "C" char fab_first(const char* x)
{
    return x[0];
}

extern "C" char fab_last(const char* x)
{
    return x[strlen(x) - 1];
}

extern "C" char* fab_to_upper(const char* x)
{
    if (x == nullptr) return nullptr;

    size_t len = strlen(x);

    char* r = static_cast<char*>(malloc(len + 1));
    if (r == nullptr) return nullptr;

    for (size_t i = 0; i < len; i++)
    {
        r[i] = static_cast<char>(toupper(static_cast<unsigned char>(x[i])));
    }

    r[len] = '\0';

    return r;
}

extern "C" char* fab_to_lower(const char* x)
{
    if (x == nullptr) return nullptr;

    size_t len = strlen(x);

    char* r = static_cast<char*>(malloc(len + 1));
    if (r == nullptr) return nullptr;

    for (size_t i = 0; i < len; i++)
    {
        r[i] = static_cast<char>(tolower(static_cast<unsigned char>(x[i])));
    }

    r[len] = '\0';

    return r;
}

extern "C" char* fab_replace(const char* x, const char* y, const char* z)
{
    if (!x || !y || !z || *y == '\0') return nullptr;

    size_t x_len = strlen(x);
    size_t y_len = strlen(y);
    size_t z_len = strlen(z);

    size_t count = 0;
    const char* tmp = x;
    while ((tmp = strstr(tmp, y))) {
        count++;
        tmp += y_len;
    }

    size_t result_len = x_len + count * (z_len - y_len);
    char* result = (char*)malloc(result_len + 1);
    if (!result) return nullptr;

    char* p = result;
    while (*x != '\0') {
        if (strstr(x, y) == x) {
            strcpy(p, z);
            p += z_len;
            x += y_len;
        } else {
            *p++ = *x++;
        }
    }
    *p = '\0';

    return result;
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
    return strtol(x, &p, 10);
}

extern "C" long long fab_to_long(const char* x)
{
    char *p = nullptr;
    return strtoll(x, &p, 10);
}

extern "C" double fab_to_double(const char* x)
{
    char *p = nullptr;
    return strtod(x, &p);
}

extern "C" bool fab_to_bool(const char* x)
{
    return x != nullptr && strcmp(x, "true") == 0;
}

extern "C" bool fab_is_blank(const char* x)
{
    for (; *x; x++)
        if (!isspace(static_cast<unsigned char>(*x))) return false;
    return true;
}

extern "C" bool fab_is_numeric(const char* x)
{
    if (x == nullptr || *x == '\0') return false;

    for (; *x; x++)
        if (!isdigit(static_cast<unsigned char>(*x))) return false;
    return true;
}

extern "C" char* fab_trim(const char* x)
{
    if (x == nullptr) return nullptr;

    while (isspace(static_cast<unsigned char>(*x))) x++;

    size_t len = strlen(x);
    while (len > 0 && isspace(static_cast<unsigned char>(x[len - 1]))) len--;

    return str_dup_n(x, len);
}

extern "C" char* fab_repeat(const char* x, int n)
{
    if (x == nullptr || n <= 0) return str_dup_n("", 0);

    size_t len = strlen(x);
    char* r = (char*)malloc(len * (size_t)n + 1);
    if (r == nullptr) return nullptr;

    for (int i = 0; i < n; i++)
        memcpy(r + len * (size_t)i, x, len);

    r[len * (size_t)n] = '\0';
    return r;
}

extern "C" char* fab_reverse(const char* x)
{
    if (x == nullptr) return nullptr;

    size_t len = strlen(x);
    char* r = str_dup_n(x, len);
    if (r == nullptr) return nullptr;

    for (size_t i = 0; i < len / 2; i++)
    {
        char t = r[i];
        r[i] = r[len - 1 - i];
        r[len - 1 - i] = t;
    }
    return r;
}

extern "C" int fab_count(const char* x, const char* y)
{
    if (x == nullptr || y == nullptr || *y == '\0') return 0;

    int c = 0;
    size_t y_len = strlen(y);
    for (const char* p = x; (p = strstr(p, y)); p += y_len) c++;

    return c;
}

extern "C" char fab_at(const char* x, int i)
{
    if (x == nullptr || i < 0 || i >= (int)strlen(x)) return '\0';
    return x[i];
}

extern "C" int fab_rfinds(const char* x, const char* y)
{
    const char* last = nullptr;
    for (const char* p = x; (p = strstr(p, y)); p++) last = p;

    return last ? (int)(last - x) : -1;
}

extern "C" char* fab_pad_left(const char* x, int width, char fill)
{
    if (x == nullptr) return nullptr;

    size_t len = strlen(x);
    if (width <= 0 || (size_t)width <= len) return str_dup_n(x, len);

    char* r = (char*)malloc((size_t)width + 1);
    if (r == nullptr) return nullptr;

    size_t pad = (size_t)width - len;
    memset(r, fill, pad);
    memcpy(r + pad, x, len + 1);   // +1 copies the '\0'

    return r;
}

extern "C" char* fab_capitalize(const char* x)
{
    if (x == nullptr) return nullptr;

    char* r = str_dup_n(x, strlen(x));
    if (r != nullptr && *r != '\0')
        r[0] = static_cast<char>(toupper(static_cast<unsigned char>(r[0])));

    return r;
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