//
// Created by Xomagat on 04.11.2026
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"

// Memory is taken from the system in 1 MB chunks and handed out by moving a pointer.
// Requests above 64 KB go straight to malloc, so a big string does not waste the rest of a chunk.
// The program is single-threaded, so there are no locks.
#define FAB_BUMP_CHUNK (1u << 20)
#define FAB_BUMP_LARGE (64u << 10)

// Constant-initialized: no startup code needed
extern "C" [[noreturn]] void fab_oom()
{
    fputs("fatal: out of memory\n", stderr);
    exit(1);
}

extern "C" void* fab_alloc_slow(size_t n)
{
    if (n > FAB_BUMP_LARGE)
    {
        void* p = malloc(n);
        if (!p) fab_oom();
        return p;
    }

    char* chunk = (char*)malloc(FAB_BUMP_CHUNK);
    if (!chunk) fab_oom();

    fab_bump_cur  = chunk + n;
    fab_bump_left = FAB_BUMP_CHUNK - n;
    return chunk;
}

extern "C" void* fab_alloc(size_t n) { return bump_alloc(n); }

// Concatenates two strings. null is treated as an empty string.
// If one part is empty, the other is returned without creating a copy.
// IMPORTANT: Do not mark this with 'noalias', as the result may be the same as one of the arguments.
extern "C" char* fab_concat2(const char* a, const char* b)
{
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    if (lb == 0) return (char*)a;
    if (la == 0) return (char*)b;

    char* r = (char*)bump_alloc(la + lb + 1);
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}