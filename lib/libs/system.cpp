//
// Created by Xomagat on 21.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#endif

extern "C" int __chkstk = 0;

extern "C" void* fab_list_new_string();
extern "C" void  fab_list_addend_string(void* l, const char* v);

static char* dup_n(const char* s, size_t n)
{
    char* r = (char*)malloc(n + 1);
    if (!r) return nullptr;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}
static char* dup_s(const char* s) { return dup_n(s, strlen(s)); }

// ---------- args ----------
// Fab main() has no argc/argv, so we catch them before main
static int    g_argc = 0;
static char** g_argv = nullptr;

#ifdef _WIN32
static void capture_args() { g_argc = __argc; g_argv = __argv; }
#else
__attribute__((constructor))
static void capture_args(int argc, char** argv, char**) { g_argc = argc; g_argv = argv; }
#endif

extern "C" int fab_arg_count()
{
#ifdef _WIN32
    if (!g_argv) capture_args();
#endif
    return g_argc;
}

extern "C" const char* fab_arg(int i)
{
#ifdef _WIN32
    if (!g_argv) capture_args();
#endif
    return (i >= 0 && i < g_argc) ? g_argv[i] : "";
}

extern "C" void* fab_args()
{
    void* l = fab_list_new_string();
    for (int i = 0; i < fab_arg_count(); ++i)
        fab_list_addend_string(l, dup_s(fab_arg(i)));
    return l;
}

// ---------- run ----------
extern "C" int fab_system(const char* cmd)
{
    if (cmd == nullptr) return -1;
    fflush(stdout);                       // otherwise our output comes after the child's
    int r = system(cmd);
#ifndef _WIN32
    if (r != -1 && WIFEXITED(r)) return WEXITSTATUS(r);
#endif
    return r;
}

// Runs a command and returns everything it printed to stdout
extern "C" char* fab_system_output(const char* cmd)
{
    if (!cmd) return dup_n("", 0);
    fflush(stdout);
#ifdef _WIN32
    FILE* p = _popen(cmd, "r");
#else
    FILE* p = popen(cmd, "r");
#endif
    if (!p) return dup_n("", 0);

    size_t cap = 256, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return dup_n("", 0);

    for (;;)
    {
        if (len + 1 >= cap)
        {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return dup_n("", 0); }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, p);
        if (n == 0) break;
        len += n;
    }
    buf[len] = '\0';
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    return buf;
}

// ---------- exit ----------
extern "C" void fab_exit(int code) { exit(code); }
extern "C" void fab_abort()        { abort(); }

// ---------- env ----------
extern "C" const char* fab_getenv(const char* name)
{
    const char* v = name ? getenv(name) : nullptr;
    return v ? v : "";
}

extern "C" bool fab_has_env(const char* name) { return name && getenv(name) != nullptr; }

extern "C" bool fab_setenv(const char* name, const char* value)
{
    if (!name || !value || !*name) return false;
#ifdef _WIN32
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, 1) == 0;
#endif
}

// ---------- time / sleep ----------
extern "C" void fab_sleep_ms(int ms)
{
    if (ms <= 0) return;
    fflush(stdout);
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    timespec ts{ ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, nullptr);
#endif
}

extern "C" void fab_sleep(int sec) { fab_sleep_ms(sec * 1000); }

// Monotonic timer, for measuring code (not a calendar time)
extern "C" long long fab_ticks_ms()
{
#ifdef _WIN32
    return (long long)GetTickCount64();
#else
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

// ---------- info ----------
extern "C" const char* fab_platform()
{
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

extern "C" int fab_cpu_count()
{
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

extern "C" int fab_pid()
{
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

extern "C" char* fab_hostname()
{
    char buf[256] = "";
#ifdef _WIN32
    DWORD n = sizeof buf; GetComputerNameA(buf, &n);
#else
    gethostname(buf, sizeof buf - 1);
#endif
    return dup_s(buf);
}

extern "C" char* fab_username()
{
    const char* u = getenv("USER");
    if (!u) u = getenv("USERNAME");
    return dup_s(u ? u : "");
}

// ---------- directories ----------
extern "C" char* fab_cwd()
{
    char buf[4096];
#ifdef _WIN32
    if (!_getcwd(buf, sizeof buf)) buf[0] = '\0';
#else
    if (!getcwd(buf, sizeof buf)) buf[0] = '\0';
#endif
    return dup_s(buf);
}

extern "C" bool fab_chdir(const char* p)
{
    if (!p) return false;
#ifdef _WIN32
    return _chdir(p) == 0;
#else
    return chdir(p) == 0;
#endif
}

extern "C" bool fab_mkdir(const char* p)
{
    if (!p) return false;
#ifdef _WIN32
    return _mkdir(p) == 0;
#else
    return mkdir(p, 0777) == 0;
#endif
}

extern "C" bool fab_rmdir(const char* p)
{
    if (!p) return false;
#ifdef _WIN32
    return _rmdir(p) == 0;
#else
    return rmdir(p) == 0;
#endif
}

extern "C" bool fab_is_dir(const char* p)
{
    if (!p) return false;
#ifdef _WIN32
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}