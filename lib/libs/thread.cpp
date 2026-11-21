//
// Created by Xomagat on 11.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef void* (*FabFn)(void*);

struct FabThread
{
    alignas(16) unsigned char result[16];
    const char*      ret_type;
    std::atomic<int> refs;
    std::atomic<int> done;
    bool             joined;
#ifdef _WIN32
    HANDLE    h;
#else
    pthread_t t;
#endif
};

static void thread_fail(const char* msg)
{
    fflush(stdout);
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void release(FabThread* t)
{
    if (t->refs.fetch_sub(1) != 1) return;
#ifdef _WIN32
    if (t->h) CloseHandle(t->h);
#endif
    free(t);
}

static void join_once(FabThread* t)
{
    if (t->joined) return;
    t->joined = true;
#ifdef _WIN32
    WaitForSingleObject(t->h, INFINITE);
#else
    pthread_join(t->t, nullptr);
#endif
}

extern "C" void* fab_thread_new(const char* ret_type)
{
    auto* t = (FabThread*)calloc(1, sizeof(FabThread));
    if (!t) return nullptr;
    t->ret_type = ret_type ? ret_type : "void";
    t->refs.store(2);
    return t;
}

extern "C" void* fab_thread_result(void* h) { return ((FabThread*)h)->result; }

extern "C" void fab_thread_finish(void* h)
{
    auto* t = (FabThread*)h;
    t->done.store(1);
    release(t);
}

#ifdef _WIN32
struct Boot { FabFn fn; void* env; };
static DWORD WINAPI boot(LPVOID p) { Boot b = *(Boot*)p; free(p); b.fn(b.env); return 0; }

extern "C" void* fab_thread_start(void* h, FabFn fn, void* env)
{
    auto* t = (FabThread*)h;
    if (!t) { free(env); return nullptr; }
    auto* b = (Boot*)malloc(sizeof(Boot));
    b->fn = fn; b->env = env;
    t->h = CreateThread(nullptr, 0, boot, b, 0, nullptr);
    if (!t->h) { free(b); free(env); free(t); return nullptr; }
    return t;
}
#else
extern "C" void* fab_thread_start(void* h, FabFn fn, void* env)
{
    auto* t = (FabThread*)h;
    if (!t) { free(env); return nullptr; }
    if (pthread_create(&t->t, nullptr, fn, env) != 0) { free(env); free(t); return nullptr; }
    return t;
}
#endif

extern "C" void* fab_thread_await(void* h, const char* want)
{
    auto* t = (FabThread*)h;
    if (!t) thread_fail("Null reference: await on a null thread");

    join_once(t);

    if (want && strcmp(t->ret_type, want) != 0)
    {
        char msg[200];
        snprintf(msg, sizeof msg, "await: thread returns '%s', but '%s' was expected", t->ret_type, want);
        thread_fail(msg);
    }
    return t->result;
}

extern "C" bool fab_thread_join(void* h)
{
    auto* t = (FabThread*)h;
    if (!t || t->joined) return false;
    join_once(t);
    return true;
}

extern "C" bool fab_thread_is_running(void* h)
{
    auto* t = (FabThread*)h;
    return t && t->done.load() == 0;
}

extern "C" void fab_thread_free(void* h)
{
    auto* t = (FabThread*)h;
    if (!t) return;
#ifndef _WIN32
    if (!t->joined) pthread_detach(t->t);
#endif
    release(t);
}

#ifdef _WIN32
extern "C" long long fab_thread_self() { return GetCurrentThreadId(); }
#else
extern "C" long long fab_thread_self() { return (long long)pthread_self(); }

extern "C" void* fab_mutex_new() { auto* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)); pthread_mutex_init(m, nullptr); return m; }
extern "C" void  fab_mutex_lock(void* m)   { if (m) pthread_mutex_lock((pthread_mutex_t*)m); }
extern "C" void  fab_mutex_unlock(void* m) { if (m) pthread_mutex_unlock((pthread_mutex_t*)m); }
extern "C" bool  fab_mutex_try_lock(void* m) { return m && pthread_mutex_trylock((pthread_mutex_t*)m) == 0; }
extern "C" void  fab_mutex_free(void* m) { if (m) { pthread_mutex_destroy((pthread_mutex_t*)m); free(m); } }
#endif