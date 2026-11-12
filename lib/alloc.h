//
// Created by Xomagat on 04.11.2026
//

#pragma once
#include <stddef.h>
#include <stdlib.h>

// Bump allocator for memory that is never freed (strings).
// Fast path is inline: a compare, an add and a subtract. The state lives in alloc.cpp.
// DO NOT use it for anything that is passed to free() or realloc() (lists, dicts, objects).
extern "C"
{
inline thread_local char*  fab_bump_cur  = nullptr;
inline thread_local size_t fab_bump_left = 0;
void* fab_alloc_slow(size_t n);
[[noreturn]] void fab_oom();
static inline void* fab_xmalloc(size_t n)           { void* p = malloc(n);     if (!p) fab_oom(); return p; }
static inline void* fab_xrealloc(void* p, size_t n) { void* r = realloc(p, n); if (!r) fab_oom(); return r; }
}

static inline void* bump_alloc(size_t n)
{
    if (n > (size_t)-1 - 8) fab_oom();
    n = (n + 7) & ~(size_t)7;
    if (n <= fab_bump_left)
    {
        void* p = fab_bump_cur;
        fab_bump_cur  += n;
        fab_bump_left -= n;
        return p;
    }
    return fab_alloc_slow(n);
}