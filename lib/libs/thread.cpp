//
// Created by Xomagat on 11.11.2026.
//

#include <stdlib.h>
typedef void* (*FabFn)(void*);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
struct FabThread { HANDLE h; DWORD id; };
struct Boot { FabFn fn; void* env; };
static DWORD WINAPI boot(LPVOID p) { Boot b = *(Boot*)p; free(p); b.fn(b.env); return 0; }

extern "C" void* fab_thread_spawn(FabFn fn, void* env)
{
    auto* t = (FabThread*)malloc(sizeof(FabThread));
    auto* b = (Boot*)malloc(sizeof(Boot)); b->fn = fn; b->env = env;
    t->h = CreateThread(nullptr, 0, boot, b, 0, &t->id);
    if (!t->h) { free(b); free(t); return nullptr; }
    return t;
}
extern "C" bool fab_thread_join(void* h)
{
    auto* t = (FabThread*)h; if (!t || !t->h) return false;
    WaitForSingleObject(t->h, INFINITE); CloseHandle(t->h); t->h = nullptr; return true;
}
extern "C" bool fab_thread_is_running(void* h)
{
    auto* t = (FabThread*)h; return t && t->h && WaitForSingleObject(t->h, 0) == WAIT_TIMEOUT;
}
extern "C" void fab_thread_free(void* h)
{
    auto* t = (FabThread*)h; if (!t) return;
    if (t->h) CloseHandle(t->h);
    free(t);
}
extern "C" long long fab_thread_self() { return GetCurrentThreadId(); }

extern "C" void* fab_mutex_new() { auto* m = (SRWLOCK*)malloc(sizeof(SRWLOCK)); InitializeSRWLock(m); return m; }
extern "C" void  fab_mutex_lock(void* m)   { if (m) AcquireSRWLockExclusive((SRWLOCK*)m); }
extern "C" void  fab_mutex_unlock(void* m) { if (m) ReleaseSRWLockExclusive((SRWLOCK*)m); }
extern "C" bool  fab_mutex_try_lock(void* m) { return m && TryAcquireSRWLockExclusive((SRWLOCK*)m); }
extern "C" void  fab_mutex_free(void* m) { free(m); }
#else
#include <pthread.h>
struct FabThread { pthread_t t; bool joined, running_flag; };

extern "C" void* fab_thread_spawn(FabFn fn, void* env)
{
    auto* t = (FabThread*)malloc(sizeof(FabThread)); t->joined = false;
    if (pthread_create(&t->t, nullptr, fn, env) != 0) { free(t); return nullptr; }
    return t;
}
extern "C" bool fab_thread_join(void* h)
{
    auto* t = (FabThread*)h; if (!t || t->joined) return false;
    t->joined = true; return pthread_join(t->t, nullptr) == 0;
}
extern "C" bool fab_thread_is_running(void* h)
{
    auto* t = (FabThread*)h; return t && !t->joined && pthread_tryjoin_np(t->t, nullptr) != 0;
}
extern "C" void fab_thread_free(void* h)
{
    auto* t = (FabThread*)h; if (!t) return;
    if (!t->joined) pthread_detach(t->t);
    free(t);
}
extern "C" long long fab_thread_self() { return (long long)pthread_self(); }

extern "C" void* fab_mutex_new() { auto* m = (pthread_mutex_t*)malloc(sizeof *m); pthread_mutex_init(m, nullptr); return m; }
extern "C" void  fab_mutex_lock(void* m)   { if (m) pthread_mutex_lock((pthread_mutex_t*)m); }
extern "C" void  fab_mutex_unlock(void* m) { if (m) pthread_mutex_unlock((pthread_mutex_t*)m); }
extern "C" bool  fab_mutex_try_lock(void* m) { return m && pthread_mutex_trylock((pthread_mutex_t*)m) == 0; }
extern "C" void  fab_mutex_free(void* m) { if (m) { pthread_mutex_destroy((pthread_mutex_t*)m); free(m); } }
#endif