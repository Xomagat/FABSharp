//
// Created by Xomagat on 14.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#define FAB_TRY_BYTES 512

struct FabTry
{
    jmp_buf     jb;
    FabTry*     prev;
    int         type_id;
    void*       payload;
    const char* msg;
};

static_assert(sizeof(FabTry)  <= FAB_TRY_BYTES, "FabTry does not fit into the alloca of the compiler: raise FAB_TRY_BYTES in both places");
static_assert(alignof(FabTry) <= 16,            "FabTry alignment is too big");

static thread_local FabTry* g_top = nullptr;

extern "C" void fab_try_push(FabTry* t)
{
    t->prev = g_top; t->type_id = 0; t->payload = nullptr; t->msg = "";
    g_top = t;
}
extern "C" void fab_try_pop() { if (g_top) g_top = g_top->prev; }

extern "C" int         fab_exc_type(FabTry* t)    { return t->type_id; }
extern "C" void*       fab_exc_payload(FabTry* t) { return t->payload; }
extern "C" const char* fab_exc_msg(FabTry* t)     { return t->msg; }

extern "C" [[noreturn]] void fab_throw(int type_id, void* payload, void* msg)
{
    FabTry* t = g_top;
    if (!t)
    {
        fflush(stdout);
        fprintf(stderr, "%s\n", (const char*)msg);
        exit(1);
    }
    g_top = t->prev;
    t->type_id = type_id;
    t->payload = payload;
    t->msg     = (const char*)msg;
    longjmp(t->jb, 1);
}

extern "C" [[noreturn]] void fab_rethrow(FabTry* t)
{
    fab_throw(t->type_id, t->payload, (void*)t->msg);
}