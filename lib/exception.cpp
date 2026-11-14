//
// Created by Xomagat on 14.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>

extern "C" [[noreturn]] void fab_throw(int type_id, void* payload, void* msg)
{
    fflush(stdout);
    fprintf(stderr, "%s", msg);
    fputc('\n', stderr);
    exit(1);
}