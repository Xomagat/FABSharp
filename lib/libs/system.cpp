//
// Created by Xomagat on 21.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

extern "C" int fab_system(const char* cmd)
{
    if (cmd == nullptr) return -1;
    int r = system(cmd);
#ifndef _WIN32
    if (r != -1 && WIFEXITED(r)) return WEXITSTATUS(r);
#endif
    return r;
}