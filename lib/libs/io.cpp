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

// The `file` type: an opaque pointer to FabFile (null = not opened / error).
// std files (stdin/stdout/stderr) are static and are never really freed.
struct FabFile
{
    FILE*       f;
    const char* path;
    const char* mode;
    bool        is_std;
};

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
static void set_msg(const char* msg)    { snprintf(g_err, sizeof g_err, "%s", msg); }

static FILE* F(void* h) { return h ? ((FabFile*)h)->f : nullptr; }

static FILE* need(void* h)
{
    FILE* f = F(h);
    if (!f) set_msg("file is not open");
    return f;
}

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

// Reads everything up to EOF (works for non-seekable streams too)
static char* read_rest(FILE* f)
{
    size_t cap = 256, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return nullptr;

    for (;;)
    {
        if (len + 1 >= cap)
        {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return nullptr; }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, f);
        if (n == 0) break;
        len += n;
    }
    buf[len] = '\0';
    return buf;
}

static void* lines_from(FILE* f)
{
    void* list = fab_list_new_string();
    while (char* s = read_line_from(f))
        fab_list_addend_string(list, s);
    return list;
}

// Open / std streams / free
extern "C" void* fab_file_open(const char* p, const char* m)
{
    if (!p || !m) { set_msg("file_open: null argument"); return nullptr; }

    FILE* f = fopen(p, m);
    if (!f) { set_errno(p); return nullptr; }

    FabFile* h = (FabFile*)malloc(sizeof(FabFile));
    if (!h) { fclose(f); set_msg("out of memory"); return nullptr; }

    h->f      = f;
    h->path   = dup_n(p, strlen(p));
    h->mode   = dup_n(m, strlen(m));
    h->is_std = false;
    return h;
}

static FabFile g_std[3];

static void* std_file(int i)
{
    FabFile& s = g_std[i];
    if (!s.f)
    {
        s.f      = (i == 0) ? stdin : (i == 1) ? stdout : stderr;
        s.path   = (i == 0) ? "<stdin>" : (i == 1) ? "<stdout>" : "<stderr>";
        s.mode   = (i == 0) ? "r" : "w";
        s.is_std = true;
    }
    return &s;
}

extern "C" void* fab_file_stdin()  { return std_file(0); }
extern "C" void* fab_file_stdout() { return std_file(1); }
extern "C" void* fab_file_stderr() { return std_file(2); }

// Used by `delete f;` and by class destructors. Null-safe, closes if still open.
extern "C" void fab_file_free(void* h)
{
    if (!h) return;
    FabFile* x = (FabFile*)h;
    if (x->is_std) return;

    if (x->f) fclose(x->f);
    free((void*)x->path);
    free((void*)x->mode);
    free(x);
}

// State
extern "C" bool fab_file_close(void* h)
{
    if (!h) { set_msg("file is not open"); return false; }
    FabFile* x = (FabFile*)h;
    if (!x->f) { set_msg("file is already closed"); return false; }

    if (x->is_std) return fflush(x->f) == 0;

    bool ok = fclose(x->f) == 0;
    x->f = nullptr;
    return ok;
}

extern "C" bool fab_file_is_open(void* h) { return F(h) != nullptr; }

// Peeks one char, so it is true right after the last line is read
extern "C" bool fab_file_eof(void* h)
{
    FILE* f = F(h);
    if (!f) return true;
    int c = fgetc(f);
    if (c == EOF) return true;
    ungetc(c, f);
    return false;
}

extern "C" bool fab_file_flush(void* h)
{
    FILE* f = need(h);
    return f && fflush(f) == 0;
}

extern "C" const char* fab_file_path(void* h) { return h ? ((FabFile*)h)->path : ""; }
extern "C" const char* fab_file_mode(void* h) { return h ? ((FabFile*)h)->mode : ""; }

// Reading
extern "C" char* fab_file_get_line(void* h)
{
    FILE* f = need(h);
    if (!f) return empty();
    char* s = read_line_from(f);
    return s ? s : empty();
}

// '\0' on EOF
extern "C" char fab_file_get_char(void* h)
{
    FILE* f = need(h);
    if (!f) return '\0';
    int c = fgetc(f);
    return c == EOF ? '\0' : (char)c;
}

extern "C" char* fab_file_read(void* h, int n)
{
    FILE* f = need(h);
    if (!f || n <= 0) return empty();

    char* r = (char*)malloc((size_t)n + 1);
    if (!r) return empty();

    size_t got = fread(r, 1, (size_t)n, f);
    r[got] = '\0';
    return r;
}

extern "C" char* fab_file_read_all(void* h)
{
    FILE* f = need(h);
    if (!f) return empty();
    char* s = read_rest(f);
    return s ? s : empty();
}

extern "C" void* fab_file_lines(void* h)
{
    FILE* f = need(h);
    if (!f) return fab_list_new_string();
    return lines_from(f);
}

// Writing
extern "C" bool fab_file_put(void* h, const char* s)
{
    FILE* f = need(h);
    return f && s && fputs(s, f) >= 0;
}

extern "C" bool fab_file_putln(void* h, const char* s)
{
    FILE* f = need(h);
    return f && s && fputs(s, f) >= 0 && fputc('\n', f) >= 0;
}

extern "C" bool fab_file_put_char(void* h, char c)
{
    FILE* f = need(h);
    return f && fputc((unsigned char)c, f) != EOF;
}

// Position
extern "C" long long fab_file_size(void* h)
{
    FILE* f = need(h);
    if (!f) return -1;

    long cur = ftell(f);
    if (cur < 0 || fseek(f, 0, SEEK_END) != 0) { set_errno("size"); return -1; }

    long long n = ftell(f);
    fseek(f, cur, SEEK_SET);
    return n;
}

extern "C" long long fab_file_tell(void* h)
{
    FILE* f = need(h);
    if (!f) return -1;
    return ftell(f);
}

extern "C" bool fab_file_seek(void* h, long long pos)
{
    FILE* f = need(h);
    if (!f) return false;
    if (fseek(f, (long)pos, SEEK_SET) == 0) return true;
    set_errno("seek");
    return false;
}

extern "C" bool fab_file_seek_i(void* h, int pos) { return fab_file_seek(h, pos); }

extern "C" void fab_file_rewind(void* h)
{
    FILE* f = need(h);
    if (f) rewind(f);
}

// Paths (no handle needed)
extern "C" bool fab_path_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

extern "C" long long fab_path_size(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { set_errno(path); return -1; }
    fseek(f, 0, SEEK_END);
    long long n = ftell(f);
    fclose(f);
    return n;
}

extern "C" char* fab_path_read(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { set_errno(path); return empty(); }
    char* s = read_rest(f);
    fclose(f);
    return s ? s : empty();
}

static bool write_mode(const char* path, const char* text, const char* mode)
{
    FILE* f = fopen(path, mode);
    if (!f) { set_errno(path); return false; }
    bool ok = fputs(text, f) >= 0;
    return fclose(f) == 0 && ok;
}

extern "C" bool fab_path_write(const char* p, const char* t)  { return write_mode(p, t, "wb"); }
extern "C" bool fab_path_append(const char* p, const char* t) { return write_mode(p, t, "ab"); }

extern "C" void* fab_path_lines(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) { set_errno(path); return fab_list_new_string(); }
    void* list = lines_from(f);
    fclose(f);
    return list;
}

extern "C" bool fab_path_delete(const char* path)
{
    if (remove(path) == 0) return true;
    set_errno(path);
    return false;
}

extern "C" bool fab_path_rename(const char* from, const char* to)
{
    if (rename(from, to) == 0) return true;
    set_errno(from);
    return false;
}

extern "C" const char* fab_io_error() { return g_err; }