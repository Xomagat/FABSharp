//
// Created by Xomagat on 09.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

template <class K, class V> struct FabDict {
    K*   keys;
    V*   vals;
    int* index;
    int  len, cap, icap;
};

template<class K, class V> FabDict<K,V>* dict_new()
{
    auto* d = (FabDict<K,V>*)malloc(sizeof(FabDict<K,V>));
    d->keys = nullptr; d->vals = nullptr; d->index = nullptr;
    d->len = d->cap = d->icap = 0;
    return d;
}

template<class K, class V> void dict_grow(FabDict<K,V>* d)
{
    d->cap  = d->cap ? d->cap * 2 : 4;
    d->icap = d->cap * 2;

    d->keys = (K*)realloc(d->keys, d->cap * sizeof(K));
    d->vals = (V*)realloc(d->vals, d->cap * sizeof(V));

    free(d->index);
    d->index = (int*)malloc(d->icap * sizeof(int));
    memset(d->index, 0xFF, d->icap * sizeof(int));

    for (int i = 0; i < d->len; ++i)
        dict_index_put(d, i);
}

inline const char* dict_own(const char* s)
{
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    memcpy(p, s, n);
    return p;
}
template<class T> T dict_own(T x) { return x; }

inline unsigned hash_key(long long x) {
    unsigned long long z = (unsigned long long)x + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return (unsigned)(z ^ (z >> 31));
}
inline unsigned hash_key(int x)  { return hash_key((long long)x); }
inline unsigned hash_key(char x) { return hash_key((long long)x); }
inline unsigned hash_key(const char* s) {
    unsigned h = 2166136261u;
    for (; *s; ++s) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

template<class K, class V> void dict_index_put(FabDict<K,V>* d, int e)
{
    unsigned mask = d->icap - 1;
    unsigned i = hash_key(d->keys[e]) & mask;
    while (d->index[i] >= 0) i = (i + 1) & mask;
    d->index[i] = e;
}

inline bool eq_key(long long a, long long b)       { return a == b; }
inline bool eq_key(const char* a, const char* b)   { return strcmp(a, b) == 0; }

template<class K, class V> int dict_find(FabDict<K,V>* d, K key)
{
    if (!d->icap) return -1;
    unsigned mask = d->icap - 1;
    for (unsigned i = hash_key(key) & mask; ; i = (i + 1) & mask)
    {
        int e = d->index[i];
        if (e < 0) return -1;
        if (eq_key(d->keys[e], key)) return e;
    }
}

template<class K, class V> void dict_set(FabDict<K,V>* d, K key, V val)
{
    int e = dict_find(d, key);
    if (e >= 0) { d->vals[e] = val; return; }

    if (d->len == d->cap) dict_grow(d);
    e = d->len++;
    d->keys[e] = dict_own(key);
    d->vals[e] = val;
    d->vals[e] = val;
    dict_index_put(d, e);
}

inline void print_elem(int v)         { printf("%d", v); }
inline void print_elem(long long v)   { printf("%lld", v); }
inline void print_elem(float v)       { printf("%f", (double)v); }
inline void print_elem(double v)      { printf("%f", v); }
inline void print_elem(const char* v) { printf("%s", v); }

template<class K, class V> void dict_print(FabDict<K,V>* d)
{
    printf("{");
    for (int i = 0; i < d->len; ++i)
    {
        if (i) printf(", ");
        print_elem(d->keys[i]);
        printf(": ");
        print_elem(d->vals[i]);
    }
    printf("}");
}

#define FAB_DICT(KN, K, VN, V) \
  extern "C" void* fab_dict_new_##KN##_##VN()                 { return dict_new<K,V>(); } \
  extern "C" void  fab_dict_set_##KN##_##VN(void* d, K k, V v){ dict_set<K,V>((FabDict<K,V>*)d, k, v); } \
  extern "C" int   fab_dict_length_##KN##_##VN(void* d)       { return ((FabDict<K,V>*)d)->len; } \
  extern "C" bool  fab_dict_contains_##KN##_##VN(void* d, K k){ return dict_find<K,V>((FabDict<K,V>*)d, k) >= 0; } \
  extern "C" void  fab_dict_print_##KN##_##VN(void* d)        { dict_print<K,V>((FabDict<K,V>*)d); }

#define FAB_DICT_VALUES(KN, K) \
    FAB_DICT(KN, K, int,    int) \
    FAB_DICT(KN, K, long,   long long) \
    FAB_DICT(KN, K, short,  short) \
    FAB_DICT(KN, K, char,   char) \
    FAB_DICT(KN, K, float,  float) \
    FAB_DICT(KN, K, double, double) \
    FAB_DICT(KN, K, string, const char*) \
    FAB_DICT(KN, K, bool,   unsigned char)

FAB_DICT_VALUES(int,    int)
FAB_DICT_VALUES(long,   long long)
FAB_DICT_VALUES(short,  short)
FAB_DICT_VALUES(char,   char)
FAB_DICT_VALUES(string, const char*)