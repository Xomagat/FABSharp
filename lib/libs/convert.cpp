//
// Created by Xomagat on 24.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../alloc.h"

struct FabHex
{
    const char* val;
};

static FabHex* hex_make(const char* s)
{
    auto* h = (FabHex*)fab_xmalloc(sizeof(FabHex));
    h->val = s;
    return h;
}

extern "C" void* fab_hex_create(const char* val)
{
    return hex_make(val ? val : "");
}

extern "C" void fab_hex_free(void* h)
{
    free(h);
}

extern "C" const char* fab_hex_to_str(void* h)
{
    return h ? ((FabHex*)h)->val : "";
}

extern "C" void* fab_convert_to_hex(const char* v, int from)
{
    if (!v || from < 2 || from > 36) return nullptr;

    char* end = nullptr;
    unsigned long long n = strtoull(v, &end, from);
    if (end == v || *end != '\0') return nullptr;

    char buf[17];
    char* p = buf + sizeof buf;
    *--p = '\0';
    do { *--p = "0123456789abcdef"[n & 15]; n >>= 4; } while (n);

    size_t len = (size_t)(buf + sizeof buf - p);
    char* r = (char*)bump_alloc(len);
    memcpy(r, p, len);
    return hex_make(r);
}