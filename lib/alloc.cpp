//
// Created by Xomagat on 04.11.2026
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>

#include "alloc.h"

// Memory is taken from the system in 1 MB chunks and handed out by moving a pointer.
// Requests above 64 KB go straight to malloc, so a big string does not waste the rest of a chunk.
// The program is single-threaded, so there are no locks.
#define FAB_BUMP_CHUNK (1u << 20)
#define FAB_BUMP_LARGE (64u << 10)

// Constant-initialized: no startup code needed
extern "C"
{
char*  fab_bump_cur  = nullptr;
size_t fab_bump_left = 0;
}

extern "C" void* fab_alloc_slow(size_t n)
{
    if (n > FAB_BUMP_LARGE) return malloc(n);

    char* chunk = (char*)malloc(FAB_BUMP_CHUNK);
    if (!chunk) return nullptr;

    // the tail of the previous chunk (less than 64 KB) is simply abandoned
    fab_bump_cur  = chunk + n;
    fab_bump_left = FAB_BUMP_CHUNK - n;
    return chunk;
}

// For code emitted by the compiler (string concatenation)
extern "C" void* fab_alloc(size_t n) { return bump_alloc(n); }