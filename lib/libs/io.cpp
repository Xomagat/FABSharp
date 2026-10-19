//
// Created by Xomagat on 19.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern "C" void* fab_list_new_string();
extern "C" void  fab_list_addend_string(void* l, const char* v);

// Helpers
static char* dup_n(const char* s, size_t n)
{
    char* r = (char*)malloc(n + 1);
    if (!r) return nullptr;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static char* empty() { return dup_n("", 0); }

static char g_err[160] = "";
static void set_errno(const char* what) { snprintf(g_err, sizeof g_err, "%s: %s", what, strerror(errno)); }

static FILE* H(long long h) { return (FILE*)(size_t)h; }

// Reads a line of any length, strips \n and \r\n. Returns nullptr on EOF.
static char* read_line_from(FILE* f)
{
    size_t cap = 128, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return nullptr;

    int c;
    while ((c = fgetc(f)) != EOF && c != '\n')
    {
        if (len + 2 > cap)
        {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return nullptr; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }

    if (c == EOF && len == 0) { free(buf); return nullptr; }
    if (len && buf[len - 1] == '\r') len--;
    buf[len] = '\0';
    return buf;
}

// Whole files
extern "C" bool fab_file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

extern "C" long long fab_file_size(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { set_errno(path); return -1; }
    fseek(f, 0, SEEK_END);
    long long n = ftell(f);
    fclose(f);
    return n;
}

extern "C" char* fab_file_read(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { set_errno(path); return empty(); }

    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return empty(); }

    char* r = (char*)malloc((size_t)n + 1);
    if (!r) { fclose(f); return empty(); }

    size_t got = fread(r, 1, (size_t)n, f);
    fclose(f);
    r[got] = '\0';
    return r;
}

static bool write_mode(const char* path, const char* text, const char* mode)
{
    FILE* f = fopen(path, mode);
    if (!f) { set_errno(path); return false; }
    bool ok = fputs(text, f) >= 0;
    return fclose(f) == 0 && ok;
}

extern "C" bool fab_file_write(const char* p, const char* t)  { return write_mode(p, t, "wb"); }
extern "C" bool fab_file_append(const char* p, const char* t) { return write_mode(p, t, "ab"); }

extern "C" void* fab_file_lines(const char* path)
{
    void* list = fab_list_new_string();
    FILE* f = fopen(path, "rb");
    if (!f) { set_errno(path); return list; }

    while (char* s = read_line_from(f))
        fab_list_addend_string(list, s);

    fclose(f);
    return list;
}

extern "C" bool fab_file_delete(const char* path)
{
    if (remove(path) == 0) return true;
    set_errno(path);
    return false;
}

extern "C" bool fab_file_rename(const char* from, const char* to)
{
    if (rename(from, to) == 0) return true;
    set_errno(from);
    return false;
}

// Handles: FILE* <-> long long (0 = error)
extern "C" long long fab_file_open(const char* p, const char* m)
{
    FILE* f = fopen(p, m);
    if (!f) set_errno(p);
    return (long long)(size_t)f;
}

extern "C" bool fab_file_close(long long h) { return h && fclose(H(h)) == 0; }

// Peeks one char, so it is true right after the last line is read
extern "C" bool fab_file_eof(long long h)
{
    if (!h) return true;
    int c = fgetc(H(h));
    if (c == EOF) return true;
    ungetc(c, H(h));
    return false;
}

extern "C" char* fab_file_get_line(long long h)
{
    if (!h) return empty();
    char* s = read_line_from(H(h));
    return s ? s : empty();
}

extern "C" bool fab_file_put(long long h, const char* s)
{
    return h && fputs(s, H(h)) >= 0;
}

extern "C" bool fab_file_putln(long long h, const char* s)
{
    return h && fputs(s, H(h)) >= 0 && fputc('\n', H(h)) >= 0;
}

extern "C" const char* fab_io_error() { return g_err; }