//
// Created by Xomagat on 07.11.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../alloc.h"

extern "C" int         fab_list_length_string(void* l);
extern "C" const char* fab_list_get_string(void* l, int i);
extern "C" bool        fab_file_put(void* f, const char* s);

struct FabSb { char* d; size_t len, cap; };

extern "C" void* fab_sb_new()
{
    auto* s = (FabSb*)malloc(sizeof(FabSb));
    if (!s) return nullptr;
    s->cap = 64; s->len = 0;
    s->d = (char*)malloc(s->cap);
    if (!s->d) { free(s); return nullptr; }
    s->d[0] = '\0';
    return s;
}

static bool sb_reserve(FabSb* s, size_t add)
{
    size_t need = s->len + add + 1;
    if (need <= s->cap) return true;
    size_t c = s->cap;
    while (c < need) c *= 2;
    char* nd = (char*)realloc(s->d, c);
    if (!nd) return false;
    s->d = nd; s->cap = c;
    return true;
}

static bool sb_put(FabSb* s, const char* p, size_t n)
{
    if (!s || !sb_reserve(s, n)) return false;
    memcpy(s->d + s->len, p, n);
    s->len += n;
    s->d[s->len] = '\0';
    return true;
}

extern "C" void fab_sb_append(void* h, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s || !t) return;
    size_t n = strlen(t);
    if (!sb_reserve(s, n)) return;
    memcpy(s->d + s->len, t, n + 1);
    s->len += n;
}

extern "C" void fab_sb_append_char(void* h, char c)
{
    auto* s = (FabSb*)h;
    if (!s || !sb_reserve(s, 1)) return;
    s->d[s->len++] = c;
    s->d[s->len] = '\0';
}

extern "C" int  fab_sb_length(void* h) { return h ? (int)((FabSb*)h)->len : 0; }
extern "C" void fab_sb_clear(void* h)  { auto* s = (FabSb*)h; if (s) { s->len = 0; s->d[0] = '\0'; } }

extern "C" const char* fab_sb_to_str(void* h)
{
    auto* s = (FabSb*)h;
    if (!s) return "";
    char* r = (char*)bump_alloc(s->len + 1);
    if (!r) return "";
    memcpy(r, s->d, s->len + 1);
    return r;
}

extern "C" void fab_sb_free(void* h)
{
    auto* s = (FabSb*)h;
    if (!s) return;
    free(s->d);
    free(s);
}

extern "C" void* fab_sb_from(const char* t)
{
    void* h = fab_sb_new();
    if (h && t) sb_put((FabSb*)h, t, strlen(t));
    return h;
}

extern "C" void* fab_sb_with_capacity(int n)
{
    auto* s = (FabSb*)fab_sb_new();
    if (s && n > 64)
    {
        char* nd = (char*)realloc(s->d, (size_t)n + 1);
        if (nd) { s->d = nd; s->cap = (size_t)n + 1; }
    }
    return s;
}

extern "C" void* fab_sb_copy(void* h)
{
    auto* s = (FabSb*)h;
    if (!s) return nullptr;
    auto* r = (FabSb*)fab_sb_new();
    if (r) sb_put(r, s->d, s->len);
    return r;
}

static void sb_put_i64(FabSb* s, long long x)
{
    char buf[24];
    char* e = buf + sizeof buf;
    unsigned long long u = x < 0 ? 0ULL - (unsigned long long)x : (unsigned long long)x;
    do { *--e = (char)('0' + u % 10); u /= 10; } while (u);
    if (x < 0) *--e = '-';
    sb_put(s, e, (size_t)(buf + sizeof buf - e));
}

static void sb_put_fp(FabSb* s, const char* fmt, double v)
{
    char buf[48];
    int n = snprintf(buf, sizeof buf, fmt, v);
    if (n > 0) sb_put(s, buf, (size_t)(n < (int)sizeof buf ? n : (int)sizeof buf - 1));
}

extern "C" void fab_sb_append_short(void* h, short x)      { sb_put_i64((FabSb*)h, x); }
extern "C" void fab_sb_append_int(void* h, int x)          { sb_put_i64((FabSb*)h, x); }
extern "C" void fab_sb_append_long(void* h, long long x)   { sb_put_i64((FabSb*)h, x); }
extern "C" void fab_sb_append_float(void* h, float x)      { sb_put_fp((FabSb*)h, "%.9g", (double)x); }
extern "C" void fab_sb_append_double(void* h, double x)    { sb_put_fp((FabSb*)h, "%.17g", x); }
extern "C" void fab_sb_append_bool(void* h, unsigned char x)
{
    sb_put((FabSb*)h, (x & 1) ? "true" : "false", (x & 1) ? 4 : 5);
}

extern "C" void fab_sb_append_line(void* h, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s) return;
    if (t) sb_put(s, t, strlen(t));
    sb_put(s, "\n", 1);
}

extern "C" void fab_sb_append_repeat(void* h, const char* t, int n)
{
    auto* s = (FabSb*)h;
    if (!s || !t || n <= 0) return;
    size_t len = strlen(t);
    if (len && (size_t)n > ((size_t)-1 - s->len - 1) / len) return;   // overflow guard
    if (!sb_reserve(s, len * (size_t)n)) return;
    for (int i = 0; i < n; ++i) sb_put(s, t, len);
}

extern "C" void fab_sb_append_char_repeat(void* h, char c, int n)
{
    for (int i = 0; i < n; ++i) fab_sb_append_char(h, c);
}

extern "C" void fab_sb_append_join(void* h, void* list, const char* sep)
{
    auto* s = (FabSb*)h;
    if (!s || !list) return;
    size_t sl = sep ? strlen(sep) : 0;
    int n = fab_list_length_string(list);
    for (int i = 0; i < n; ++i)
    {
        if (i && sl) sb_put(s, sep, sl);
        const char* t = fab_list_get_string(list, i);
        if (t) sb_put(s, t, strlen(t));
    }
}

extern "C" void fab_sb_insert(void* h, int pos, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s || !t || pos < 0 || (size_t)pos > s->len) return;
    size_t n = strlen(t);
    if (n == 0 || !sb_reserve(s, n)) return;
    memmove(s->d + pos + n, s->d + pos, s->len - (size_t)pos + 1);   // + '\0'
    memcpy(s->d + pos, t, n);
    s->len += n;
}

extern "C" void fab_sb_remove(void* h, int pos, int len)
{
    auto* s = (FabSb*)h;
    if (!s || pos < 0 || len <= 0 || (size_t)pos >= s->len) return;
    size_t n = (size_t)len;
    if (n > s->len - (size_t)pos) n = s->len - (size_t)pos;
    memmove(s->d + pos, s->d + pos + n, s->len - (size_t)pos - n + 1);
    s->len -= n;
}

extern "C" void fab_sb_truncate(void* h, int n)
{
    auto* s = (FabSb*)h;
    if (!s || n < 0 || (size_t)n >= s->len) return;
    s->len = (size_t)n;
    s->d[n] = '\0';
}

extern "C" void fab_sb_reverse(void* h)
{
    auto* s = (FabSb*)h;
    if (!s) return;
    for (size_t i = 0; i < s->len / 2; ++i)
    {
        char t = s->d[i]; s->d[i] = s->d[s->len - 1 - i]; s->d[s->len - 1 - i] = t;
    }
}

extern "C" void fab_sb_set_at(void* h, int i, char c)
{
    auto* s = (FabSb*)h;
    if (s && i >= 0 && (size_t)i < s->len) s->d[i] = c;
}

// Returns the number of replacements
extern "C" int fab_sb_replace(void* h, const char* from, const char* to)
{
    auto* s = (FabSb*)h;
    if (!s || !from || !*from) return 0;
    if (!to) to = "";
    size_t fl = strlen(from), tl = strlen(to);

    int cnt = 0;
    for (const char* p = s->d; (p = strstr(p, from)); p += fl) cnt++;
    if (!cnt) return 0;

    size_t nl = s->len - (size_t)cnt * fl + (size_t)cnt * tl;
    size_t nc = nl + 1 < 64 ? 64 : nl + 1;
    char* nd = (char*)malloc(nc);
    if (!nd) return 0;

    char* o = nd;
    const char* src = s->d;
    const char* p;
    while ((p = strstr(src, from)))
    {
        memcpy(o, src, (size_t)(p - src)); o += p - src;
        memcpy(o, to, tl);                 o += tl;
        src = p + fl;
    }
    memcpy(o, src, strlen(src) + 1);

    free(s->d);
    s->d = nd; s->len = nl; s->cap = nc;
    return cnt;
}

extern "C" bool fab_sb_is_empty(void* h) { auto* s = (FabSb*)h; return !s || s->len == 0; }
extern "C" int  fab_sb_capacity(void* h) { return h ? (int)((FabSb*)h)->cap : 0; }

extern "C" char fab_sb_at(void* h, int i)
{
    auto* s = (FabSb*)h;
    return (s && i >= 0 && (size_t)i < s->len) ? s->d[i] : '\0';
}

extern "C" int fab_sb_find(void* h, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s || !t) return -1;
    const char* p = strstr(s->d, t);
    return p ? (int)(p - s->d) : -1;
}

extern "C" bool fab_sb_contains(void* h, const char* t) { return fab_sb_find(h, t) >= 0; }

extern "C" bool fab_sb_starts_with(void* h, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s || !t) return false;
    size_t n = strlen(t);
    return n <= s->len && memcmp(s->d, t, n) == 0;
}

extern "C" bool fab_sb_ends_with(void* h, const char* t)
{
    auto* s = (FabSb*)h;
    if (!s || !t) return false;
    size_t n = strlen(t);
    return n <= s->len && memcmp(s->d + s->len - n, t, n) == 0;
}

extern "C" const char* fab_sb_substr(void* h, int start, int len)
{
    auto* s = (FabSb*)h;
    if (!s || start < 0 || len <= 0 || (size_t)start >= s->len) return "";
    size_t n = (size_t)len;
    if (n > s->len - (size_t)start) n = s->len - (size_t)start;
    char* r = (char*)bump_alloc(n + 1);
    memcpy(r, s->d + start, n);
    r[n] = '\0';
    return r;
}

extern "C" void fab_sb_print(void* h)
{
    auto* s = (FabSb*)h;
    if (s) fwrite(s->d, 1, s->len, stdout);
}
extern "C" void fab_sb_println(void* h) { fab_sb_print(h); fputc('\n', stdout); }

extern "C" bool fab_sb_write_to(void* h, void* file)
{
    auto* s = (FabSb*)h;
    return s && fab_file_put(file, s->d);
}