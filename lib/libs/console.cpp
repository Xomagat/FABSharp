//
// Created by Xomagat on 01.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

static char* dup_n(const char* s, size_t n)
{
    char* r = (char*)malloc(n + 1);
    if (!r) return nullptr;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static void enable_vt()
{
#ifdef _WIN32
    static bool done = false;
    if (done) return;
    done = true;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    if (GetConsoleMode(h, &m)) SetConsoleMode(h, m | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
}

extern "C" void fab_con_print(const char* s)    { if (s) fputs(s, stdout); }
extern "C" void fab_con_println(const char* s)  { if (s) fputs(s, stdout); fputc('\n', stdout); }
extern "C" void fab_con_error(const char* s)    { if (s) fputs(s, stderr); }
extern "C" void fab_con_errorln(const char* s)  { if (s) fputs(s, stderr); fputc('\n', stderr); }
extern "C" void fab_con_flush()                 { fflush(stdout); }

extern "C" char* fab_con_read_line()
{
    fflush(stdout);
    size_t cap = 128, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return dup_n("", 0);

    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n')
    {
        if (len + 2 > cap)
        {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return dup_n("", 0); }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (len && buf[len - 1] == '\r') len--;
    buf[len] = '\0';
    return buf;
}

extern "C" char fab_con_read_char()
{
    fflush(stdout);
    int c = fgetc(stdin);
    return c == EOF ? '\0' : (char)c;
}

extern "C" void fab_con_clear()
{
    enable_vt();
    fputs("\x1b[2J\x1b[H", stdout);
}

extern "C" void fab_con_color(int c)
{
    enable_vt();
    if (c < 0 || c > 7) return;
    printf("\x1b[%dm", 30 + c);
}

extern "C" void fab_con_reset()
{
    enable_vt();
    fputs("\x1b[0m", stdout);
}

extern "C" void fab_con_beep()  { fputc('\a', stdout); fflush(stdout); }

extern "C" void fab_con_title(const char* s)
{
    enable_vt();
    if (s) printf("\x1b]0;%s\x07", s);
}