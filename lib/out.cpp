//
// Created by Xomagat on 03.11.2026
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <string.h>

// Fast output for write / writeln. Everything goes through the normal stdout buffer,
// so the order with printf (lists, console, ldouble) is preserved.
// No format parsing: strings are written with fwrite, integers with our own itoa.
#ifdef _WIN32
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define FAB_ISATTY(fd) _isatty(fd)
#define FAB_WRITE(p, n) fwrite((p), 1, (n), stdout)
#define FAB_PUTC(c)     putc((c), stdout)

static bool stdout_is_console()
{
    DWORD m;
    return GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &m) != 0;
}
#else
#include <unistd.h>
#define FAB_ISATTY(fd) isatty(fd)
#define FAB_WRITE(p, n) fwrite_unlocked((p), 1, (n), stdout)
#define FAB_PUTC(c)     putc_unlocked((c), stdout)
static bool stdout_is_console() { return isatty(1) != 0; }
#endif

// Called once at the start of main. Not a terminal (file, pipe, benchmark) -> full 64 KB buffer.
static bool g_inited = false;

static inline void ensure_init()
{
    if (g_inited) return;
    g_inited = true;
    if (!stdout_is_console()) setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
}

extern "C" void fab_out_init() { ensure_init(); }

extern "C" void fab_out_nl() { ensure_init(); FAB_PUTC('\n'); }

// String (null prints nothing)
static inline void out_str(const char* s, bool nl)
{
    ensure_init();
    if (!s) s = "";
    if (nl) { puts(s); return; }
    FAB_WRITE(s, strlen(s));
}

extern "C" void fab_out_str(const char* s)   { ensure_init(); out_str(s, false); }
extern "C" void fab_out_strln(const char* s) { ensure_init(); out_str(s, true); }

// Char
extern "C" void fab_out_char(char c)   { ensure_init(); FAB_PUTC((unsigned char)c); }
extern "C" void fab_out_charln(char c)
{
    ensure_init();
    char b[2] = {c, '\n'};
    FAB_WRITE(b, 2);
}

// Integers (short and int arrive as i32, long as i64)
static void out_i64(long long x, bool nl)
{
    ensure_init();
    char buf[32];
    char* end = buf + sizeof buf;
    if (nl) *--end = '\n';

    unsigned long long u = x < 0 ? 0ULL - (unsigned long long)x : (unsigned long long)x;
    do { *--end = (char)('0' + u % 10); u /= 10; } while (u);
    if (x < 0) *--end = '-';

    FAB_WRITE(end, (size_t)(buf + sizeof buf - end));
}
extern "C" void fab_out_i32(int x)         { ensure_init(); out_i64(x, false); }
extern "C" void fab_out_i32ln(int x)       { ensure_init(); out_i64(x, true); }
extern "C" void fab_out_i64(long long x)   { ensure_init(); out_i64(x, false); }
extern "C" void fab_out_i64ln(long long x) { ensure_init(); out_i64(x, true); }

// Floating point: same output as before (%.17g / %.9g), but without printf's format parsing
static void out_fp(double v, const char* fmt, bool nl)
{
    ensure_init();
    char buf[48];
    int n = snprintf(buf, sizeof buf - 1, fmt, v);
    if (n < 0) return;
    if (nl) buf[n++] = '\n';
    FAB_WRITE(buf, (size_t)n);
}
extern "C" void fab_out_f64(double v)   { ensure_init(); out_fp(v, "%.17g", false); }
extern "C" void fab_out_f64ln(double v) { ensure_init(); out_fp(v, "%.17g", true); }
extern "C" void fab_out_f32(float v)    { ensure_init(); out_fp((double)v, "%.9g", false); }
extern "C" void fab_out_f32ln(float v)  { ensure_init(); out_fp((double)v, "%.9g", true); }

extern "C" void fab_input_done(int rc)
{
    if (rc == 1) return;
    int c;
    while ((c = getchar()) != EOF && c != '\n') {}
}