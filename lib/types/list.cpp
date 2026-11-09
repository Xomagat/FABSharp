//
// Created by Xomagat on 07.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../alloc.h"

// NOTE: IndexExpression reads `data` directly from the list pointer,
// so `data` must stay the first field of the struct.
template<class T> struct FabList { T* data; int len; int cap;
};

template<class T> FabList<T>* list_new()
{
    auto* l = (FabList<T>*)fab_xmalloc(sizeof(FabList<T>));
    l->data = nullptr; l->len = 0; l->cap = 0;
    return l;
}

template<class T> void list_grow(FabList<T>* l, long long need)
{
    if (need > INT_MAX) fab_oom();
    long long c = l->cap ? (long long)l->cap * 2 : 4;
    if (c < need)    c = need;
    if (c > INT_MAX) c = INT_MAX;
    l->data = (T*)fab_xrealloc(l->data, (size_t)c * sizeof(T));
    l->cap  = (int)c;
}

template<class T> void ensure_list(FabList<T>* l)
{
    if (l->len < l->cap) return;
    list_grow(l, (long long)l->len + 1);
}

template<class T> void list_reserve(FabList<T>* l, int n)
{
    if (n <= l->cap) return;
    list_grow(l, n);
}

// Helpers
// bool is stored as unsigned char (like in dict), the caller may leave garbage in the upper bits
template<class T> T list_norm(T v) { return v; }
inline unsigned char list_norm(unsigned char v) { return v & 1; }

// Value returned when an index is out of range or the list is empty
template<class T> T list_zero() { return T(); }
template<> inline const char* list_zero<const char*>() { return ""; }

template<class T> bool list_eq(T a, T b) { return a == b; }
inline bool list_eq(const char* a, const char* b)
{
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

template<class T> bool list_lt(T a, T b) { return a < b; }
inline bool list_lt(const char* a, const char* b)
{
    if (!a || !b) return a == nullptr && b != nullptr;
    return strcmp(a, b) < 0;
}

// Add
template<class T> void list_addend(FabList<T>* l, T v)
{
    ensure_list(l);
    l->data[l->len++] = v;
}

template<class T> void list_add(FabList<T>* l, T v)
{
    ensure_list(l);
    memmove(l->data + 1, l->data, l->len * sizeof(T));
    l->data[0] = v;
    l->len++;
}

template<class T> void list_insert(FabList<T>* l, int i, T v)
{
    if (i < 0 || i > l->len) return;
    ensure_list(l);
    memmove(l->data + i + 1, l->data + i, (l->len - i) * sizeof(T));
    l->data[i] = v;
    l->len++;
}

template<class T> void list_extend(FabList<T>* l, FabList<T>* o)
{
    int n = o->len;
    if (n <= 0) return;

    if ((long long)l->len + n > INT_MAX) fab_oom();
    list_reserve(l, l->len + n);
    memcpy(l->data + l->len, o->data, n * sizeof(T));
    l->len += n;
}

// Access
template<class T> T list_get(FabList<T>* l, int i)
{
    if (i < 0 || i >= l->len) return list_zero<T>();
    return l->data[i];
}

template<class T> void list_set(FabList<T>* l, int i, T v)
{
    if (i < 0 || i >= l->len) return;
    l->data[i] = v;
}

// Remove
template<class T> void list_remove_at(FabList<T>* l, int i)
{
    if (i < 0 || i >= l->len) return;
    memmove(l->data + i, l->data + i + 1, (l->len - i - 1) * sizeof(T));
    l->len--;
}

template<class T> T list_pop(FabList<T>* l)
{
    if (l->len == 0) return list_zero<T>();
    return l->data[--l->len];
}

template<class T> T list_pop_first(FabList<T>* l)
{
    if (l->len == 0) return list_zero<T>();
    T v = l->data[0];
    list_remove_at(l, 0);
    return v;
}

template<class T> void list_clear(FabList<T>* l)
{
    l->len = 0;
}

// Search
template<class T> int list_index_of(FabList<T>* l, T v)
{
    for (int i = 0; i < l->len; ++i)
        if (list_eq(l->data[i], v)) return i;
    return -1;
}

template<class T> int list_count(FabList<T>* l, T v)
{
    int c = 0;
    for (int i = 0; i < l->len; ++i)
        if (list_eq(l->data[i], v)) c++;
    return c;
}

template<class T> bool list_remove(FabList<T>* l, T v)
{
    int i = list_index_of(l, v);
    if (i < 0) return false;
    list_remove_at(l, i);
    return true;
}

// Order
template<class T> void list_reverse(FabList<T>* l)
{
    for (int i = 0; i < l->len / 2; ++i)
    {
        T t = l->data[i];
        l->data[i] = l->data[l->len - 1 - i];
        l->data[l->len - 1 - i] = t;
    }
}

template<class T> void list_swap(FabList<T>* l, int i, int j)
{
    if (i < 0 || j < 0 || i >= l->len || j >= l->len) return;
    T t = l->data[i];
    l->data[i] = l->data[j];
    l->data[j] = t;
}

template<class T> void list_insertion(T* a, int n)
{
    for (int i = 1; i < n; ++i)
    {
        T v = a[i]; int j = i - 1;
        while (j >= 0 && list_lt(v, a[j])) { a[j + 1] = a[j]; --j; }
        a[j + 1] = v;
    }
}

template<class T> void list_sift(T* a, int i, int n)
{
    for (;;)
    {
        int c = 2 * i + 1;
        if (c >= n) return;
        if (c + 1 < n && list_lt(a[c], a[c + 1])) ++c;
        if (!list_lt(a[i], a[c])) return;
        T t = a[i]; a[i] = a[c]; a[c] = t;
        i = c;
    }
}

template<class T> void list_heapsort(T* a, int n)
{
    for (int i = n / 2 - 1; i >= 0; --i) list_sift(a, i, n);
    for (int e = n - 1; e > 0; --e) { T t = a[0]; a[0] = a[e]; a[e] = t; list_sift(a, 0, e); }
}

template<class T> void list_introsort(T* a, int n, int depth)
{
    while (n > 16)
    {
        if (depth-- == 0) { list_heapsort(a, n); return; }

        T x = a[0], y = a[n / 2], z = a[n - 1];
        T p = list_lt(x, y) ? (list_lt(y, z) ? y : (list_lt(x, z) ? z : x))
                            : (list_lt(x, z) ? x : (list_lt(y, z) ? z : y));

        int i = 0, j = n - 1;
        while (i <= j)
        {
            while (list_lt(a[i], p)) ++i;
            while (list_lt(p, a[j])) --j;
            if (i <= j) { T t = a[i]; a[i] = a[j]; a[j] = t; ++i; --j; }
        }

        if (j + 1 < n - i) { list_introsort(a, j + 1, depth); a += i; n -= i; }
        else               { list_introsort(a + i, n - i, depth); n = j + 1; }
    }
    list_insertion(a, n);
}

template<class T> void list_sort(FabList<T>* l)
{
    if (l->len < 2) return;
    int d = 0;
    for (int n = l->len; n > 1; n >>= 1) d += 2;
    list_introsort(l->data, l->len, d);
}

template<class T> void list_sort_desc(FabList<T>* l)
{
    list_sort(l);
    list_reverse(l);
}

// Min, max, sum
template<class T> T list_min(FabList<T>* l)
{
    if (l->len == 0) return list_zero<T>();
    T m = l->data[0];
    for (int i = 1; i < l->len; ++i)
        if (list_lt(l->data[i], m)) m = l->data[i];
    return m;
}

template<class T> T list_max(FabList<T>* l)
{
    if (l->len == 0) return list_zero<T>();
    T m = l->data[0];
    for (int i = 1; i < l->len; ++i)
        if (list_lt(m, l->data[i])) m = l->data[i];
    return m;
}

template<class T> T sum_add(T a, T b) { return a + b; }
inline int       sum_add(int a, int b)             { return (int)((unsigned)a + (unsigned)b); }
inline long long sum_add(long long a, long long b) { return (long long)((unsigned long long)a + (unsigned long long)b); }

template<class T> T list_sum(FabList<T>* l)
{
    T s = T();
    for (int i = 0; i < l->len; ++i) s = sum_add(s, l->data[i]);
    return s;
}

// Sum of strings = concatenation
inline const char* list_sum(FabList<const char*>* l)
{
    size_t total = 1;
    for (int i = 0; i < l->len; ++i)
        if (l->data[i]) total += strlen(l->data[i]);

    char* r = (char*)bump_alloc(total);

    char* p = r;
    for (int i = 0; i < l->len; ++i)
    {
        if (!l->data[i]) continue;
        size_t n = strlen(l->data[i]);
        memcpy(p, l->data[i], n);
        p += n;
    }
    *p = '\0';

    return r;
}

// Copy, slice
template<class T> FabList<T>* list_copy(FabList<T>* l)
{
    auto* r = list_new<T>();
    if (l->len <= 0) return r;

    list_reserve(r, l->len);
    memcpy(r->data, l->data, l->len * sizeof(T));
    r->len = l->len;
    return r;
}

template<class T> FabList<T>* list_slice(FabList<T>* l, int start, int length)
{
    auto* r = list_new<T>();
    if (start < 0 || start >= l->len || length <= 0) return r;

    if (start + length > l->len)
        length = l->len - start;

    list_reserve(r, length);
    memcpy(r->data, l->data + start, length * sizeof(T));
    r->len = length;
    return r;
}

// Print
inline void print_elem(int v)           { printf("%d", v); }
inline void print_elem(short v)         { printf("%d", v); }
inline void print_elem(char v)          { printf("%c", v); }
inline void print_elem(unsigned char v) { printf("%s", v ? "true" : "false"); }
inline void print_elem(long long v)     { printf("%lld", v); }
inline void print_elem(float v)         { printf("%f", (double)v); }
inline void print_elem(double v)        { printf("%f", v); }
inline void print_elem(const char* v)   { printf("%s", v); }

template<class T> void list_print(FabList<T>* l)
{
    printf("[");
    for (int i = 0; i < l->len; ++i)
    {
        if (i) printf(", ");
        print_elem(l->data[i]);
    }
    printf("]");
}

// Join
inline int fmt_elem(char* b, size_t n, int v)           { return snprintf(b, n, "%d", v); }
inline int fmt_elem(char* b, size_t n, short v)         { return snprintf(b, n, "%d", v); }
inline int fmt_elem(char* b, size_t n, char v)          { return snprintf(b, n, "%c", v); }
inline int fmt_elem(char* b, size_t n, unsigned char v) { return snprintf(b, n, "%s", v ? "true" : "false"); }
inline int fmt_elem(char* b, size_t n, long long v)     { return snprintf(b, n, "%lld", v); }
inline int fmt_elem(char* b, size_t n, float v)         { return snprintf(b, n, "%.9g", (double)v); }
inline int fmt_elem(char* b, size_t n, double v)        { return snprintf(b, n, "%.17g", v); }
inline int fmt_elem(char* b, size_t n, const char* v)   { return snprintf(b, n, "%s", v ? v : ""); }

template<class T> char* list_join(FabList<T>* l, const char* sep)
{
    if (sep == nullptr) sep = "";
    size_t sep_len = strlen(sep);

    size_t total = 1;
    for (int i = 0; i < l->len; ++i)
        total += (size_t)fmt_elem(nullptr, 0, l->data[i]) + (i ? sep_len : 0);

    char* r = (char*)bump_alloc(total);

    char* p = r;
    for (int i = 0; i < l->len; ++i)
    {
        if (i)
        {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
        p += fmt_elem(p, total - (size_t)(p - r), l->data[i]);
    }
    *p = '\0';

    return r;
}

#define FAB_LIST(NAME, T) \
    extern "C" void*  fab_list_new_##NAME()                         { return list_new<T>(); } \
    extern "C" void   fab_list_addend_##NAME(void* l, T v)          { list_addend<T>((FabList<T>*)l, list_norm(v)); } \
    extern "C" void   fab_list_add_##NAME(void* l, T v)             { list_add<T>((FabList<T>*)l, list_norm(v)); } \
    extern "C" int    fab_list_length_##NAME(void* l)               { return ((FabList<T>*)l)->len; } \
    extern "C" int    fab_list_capacity_##NAME(void* l)             { return ((FabList<T>*)l)->cap; } \
    extern "C" bool   fab_list_empty_##NAME(void* l)                { return ((FabList<T>*)l)->len == 0; } \
    extern "C" void   fab_list_print_##NAME(void* l)                { list_print<T>((FabList<T>*)l); } \
    extern "C" void   fab_list_reserve_##NAME(void* l, int n)       { list_reserve<T>((FabList<T>*)l, n); } \
    extern "C" T      fab_list_get_##NAME(void* l, int i)           { return list_get<T>((FabList<T>*)l, i); } \
    extern "C" void   fab_list_set_##NAME(void* l, int i, T v)      { list_set<T>((FabList<T>*)l, i, list_norm(v)); } \
    extern "C" T      fab_list_first_##NAME(void* l)                { return list_get<T>((FabList<T>*)l, 0); } \
    extern "C" T      fab_list_last_##NAME(void* l)                 { return list_get<T>((FabList<T>*)l, ((FabList<T>*)l)->len - 1); } \
    extern "C" void   fab_list_insert_##NAME(void* l, int i, T v)   { list_insert<T>((FabList<T>*)l, i, list_norm(v)); } \
    extern "C" void   fab_list_remove_at_##NAME(void* l, int i)     { list_remove_at<T>((FabList<T>*)l, i); } \
    extern "C" bool   fab_list_remove_##NAME(void* l, T v)          { return list_remove<T>((FabList<T>*)l, list_norm(v)); } \
    extern "C" T      fab_list_pop_##NAME(void* l)                  { return list_pop<T>((FabList<T>*)l); } \
    extern "C" T      fab_list_pop_first_##NAME(void* l)            { return list_pop_first<T>((FabList<T>*)l); } \
    extern "C" void   fab_list_clear_##NAME(void* l)                { list_clear<T>((FabList<T>*)l); } \
    extern "C" bool   fab_list_contains_##NAME(void* l, T v)        { return list_index_of<T>((FabList<T>*)l, list_norm(v)) >= 0; } \
    extern "C" int    fab_list_index_of_##NAME(void* l, T v)        { return list_index_of<T>((FabList<T>*)l, list_norm(v)); } \
    extern "C" int    fab_list_count_##NAME(void* l, T v)           { return list_count<T>((FabList<T>*)l, list_norm(v)); } \
    extern "C" void   fab_list_reverse_##NAME(void* l)              { list_reverse<T>((FabList<T>*)l); } \
    extern "C" void   fab_list_swap_##NAME(void* l, int i, int j)   { list_swap<T>((FabList<T>*)l, i, j); } \
    extern "C" void   fab_list_sort_##NAME(void* l)                 { list_sort<T>((FabList<T>*)l); } \
    extern "C" void   fab_list_sort_desc_##NAME(void* l)            { list_sort_desc<T>((FabList<T>*)l); } \
    extern "C" T      fab_list_min_##NAME(void* l)                  { return list_min<T>((FabList<T>*)l); } \
    extern "C" T      fab_list_max_##NAME(void* l)                  { return list_max<T>((FabList<T>*)l); } \
    extern "C" void*  fab_list_copy_##NAME(void* l)                 { return list_copy<T>((FabList<T>*)l); } \
    extern "C" void*  fab_list_slice_##NAME(void* l, int s, int n)  { return list_slice<T>((FabList<T>*)l, s, n); } \
    extern "C" void   fab_list_extend_##NAME(void* l, void* o)      { list_extend<T>((FabList<T>*)l, (FabList<T>*)o); } \
    extern "C" char*  fab_list_join_##NAME(void* l, const char* sep) { return list_join<T>((FabList<T>*)l, sep); } \
    extern "C" void   fab_list_free_##NAME(void* l)                 { auto* x = (FabList<T>*)l; if (!x) return; free(x->data); free(x); }

// sum: numbers are added up, strings are concatenated (not available for char and bool)
#define FAB_LIST_SUM(NAME, T) \
    extern "C" T      fab_list_sum_##NAME(void* l)                  { return list_sum((FabList<T>*)l); }

FAB_LIST(int,    int)
FAB_LIST(long,   long long)
FAB_LIST(short,  short)
FAB_LIST(char,   char)
FAB_LIST(float,  float)
FAB_LIST(double, double)
FAB_LIST(string, const char*)
FAB_LIST(bool,   unsigned char)

FAB_LIST_SUM(int,    int)
FAB_LIST_SUM(long,   long long)
FAB_LIST_SUM(short,  short)
FAB_LIST_SUM(float,  float)
FAB_LIST_SUM(double, double)
FAB_LIST_SUM(string, const char*)