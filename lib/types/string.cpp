//
// Created by Xomagat on 05.10.2026.
//

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    char const *p = strstr(x, y);
    return p - x;
}

extern "C" char* fab_substr(const char *src, size_t start, size_t length)
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

extern "C" bool fab_contains(const char* x, const char* y)
{
    return fab_finds(x, y) == 0;
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

extern "C" long fab_to_long(const char* x)
{
    char *p = nullptr;
    return strtoll(x, &p, 10);
}

extern "C" double fab_to_double(const char* x)
{
    char *p = nullptr;
    return strtod(x, &p);
}