//
// Created by Xomagat on 07.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <string.h>

#include "../alloc.h"

struct FabSb { char* d; size_t len, cap; };

extern "C" void* fab_sb_new()
{
    auto* s = (FabSb*)malloc(sizeof(FabSb));
    if (!s) return nullptr;
    s->cap = 64; s->len = 0;
    s->d = (char*)malloc(s->cap);
    if (!s->d) { free(s); return nullptr; }
    s->d[0] = '\0';
    return s;
}

static bool sb_reserve(FabSb* s, size_t add)
{
    size_t need = s->len + add + 1;
    if (need <= s->cap) return true;
    size_t c = s->cap;
    while (c < need) c *= 2;
    char* nd = (char*)realloc(s->d, c);
    if (!nd) return false;
    s->d = nd; s->cap = c;
    return true;
}

extern "C" void fab_sb_append(void* h, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s || !t) return;
    size_t n = strlen(t);
    if (!sb_reserve(s, n)) return;
    memcpy(s->d + s->len, t, n + 1);
    s->len += n;
}

extern "C" void fab_sb_append_char(void* h, char c)
{
    auto* s = (FabSb*)h;
    if (!s || !sb_reserve(s, 1)) return;
    s->d[s->len++] = c;
    s->d[s->len] = '\0';
}

extern "C" int  fab_sb_length(void* h) { return h ? (int)((FabSb*)h)->len : 0; }
extern "C" void fab_sb_clear(void* h)  { auto* s = (FabSb*)h; if (s) { s->len = 0; s->d[0] = '\0'; } }

// Копия в bump-память: результат переживёт delete буфера
extern "C" const char* fab_sb_to_str(void* h)
{
    auto* s = (FabSb*)h;
    if (!s) return "";
    char* r = (char*)bump_alloc(s->len + 1);
    if (!r) return "";
    memcpy(r, s->d, s->len + 1);
    return r;
}

extern "C" void fab_sb_free(void* h)
{
    auto* s = (FabSb*)h;
    if (!s) return;
    free(s->d);
    free(s);
}