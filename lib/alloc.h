//
// Created by Xomagat on 04.11.2026
//

#pragma once
#include <stddef.h>

// Bump allocator for memory that is never freed (strings).
// Fast path is inline: a compare, an add and a subtract. The state lives in alloc.cpp.
// DO NOT use it for anything that is passed to free() or realloc() (lists, dicts, objects).
extern "C"
{
extern char*  fab_bump_cur;
extern size_t fab_bump_left;
void* fab_alloc_slow(size_t n);
}

static inline void* bump_alloc(size_t n)
{
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