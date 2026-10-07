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

extern "C" bool fab_clear(char* x)
{
    return *x = '\0';
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