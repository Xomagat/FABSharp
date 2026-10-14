//
// Created by Xomagat on 09.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Lists (list.cpp): keys() and values() build lists
#define FAB_LIST_IMPORT(NAME, T) \
    extern "C" void* fab_list_new_##NAME(); \
    extern "C" void  fab_list_addend_##NAME(void* l, T v);

FAB_LIST_IMPORT(int,    int)
FAB_LIST_IMPORT(long,   long long)
FAB_LIST_IMPORT(short,  short)
FAB_LIST_IMPORT(char,   char)
FAB_LIST_IMPORT(float,  float)
FAB_LIST_IMPORT(double, double)
FAB_LIST_IMPORT(string, const char*)
FAB_LIST_IMPORT(bool,   unsigned char)

// Dict keeps insertion order:
// keys[] and vals[] are plain arrays, index[] is an open-addressing hash table with entry numbers
template <class K, class V> struct FabDict {
    K*   keys;
    V*   vals;
    int* index;
    int  len, cap, icap;
};

template<class T> void dict_free_key(T) {}
inline void dict_free_key(const char* s) { free((void*)s); }

template<class K, class V> void dict_free(FabDict<K,V>* d)
{
    if (!d) return;
    for (int i = 0; i < d->len; ++i) dict_free_key(d->keys[i]);
    free(d->keys); free(d->vals); free(d->index); free(d);
}

// Hash
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

inline bool eq_key(long long a, long long b)       { return a == b; }
inline bool eq_key(const char* a, const char* b)   { return strcmp(a, b) == 0; }

// Values: equality, normalization, "empty" value
template<class T> bool dict_eq_val(T a, T b) { return a == b; }
inline bool dict_eq_val(const char* a, const char* b)
{
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

// bool is stored as unsigned char (like in list), the caller may leave garbage in the upper bits
template<class T> T dict_norm(T v) { return v; }
inline unsigned char dict_norm(unsigned char v) { return v & 1; }

// Value returned when the key is missing or the index is out of range
template<class T> T dict_zero() { return T(); }
template<> inline const char* dict_zero<const char*>() { return ""; }

// Strings keys are copied, the dict owns them
inline const char* dict_own(const char* s)
{
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    memcpy(p, s, n);
    return p;
}
template<class T> T dict_own(T x) { return x; }

// Core
template<class K, class V> FabDict<K,V>* dict_new()
{
    auto* d = (FabDict<K,V>*)malloc(sizeof(FabDict<K,V>));
    d->keys = nullptr; d->vals = nullptr; d->index = nullptr;
    d->len = d->cap = d->icap = 0;
    return d;
}

template<class K, class V> void dict_index_put(FabDict<K,V>* d, int e)
{
    unsigned mask = d->icap - 1;
    unsigned i = hash_key(d->keys[e]) & mask;
    while (d->index[i] >= 0) i = (i + 1) & mask;
    d->index[i] = e;
}

template<class K, class V> void dict_reindex(FabDict<K,V>* d)
{
    if (!d->icap) return;

    memset(d->index, 0xFF, d->icap * sizeof(int));
    for (int i = 0; i < d->len; ++i)
        dict_index_put(d, i);
}

template<class K, class V> void dict_grow(FabDict<K,V>* d)
{
    d->cap  = d->cap ? d->cap * 2 : 4;
    d->icap = d->cap * 2;

    d->keys = (K*)realloc(d->keys, d->cap * sizeof(K));
    d->vals = (V*)realloc(d->vals, d->cap * sizeof(V));

    free(d->index);
    d->index = (int*)malloc(d->icap * sizeof(int));

    dict_reindex(d);
}

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
    dict_index_put(d, e);
}

// Access
template<class K, class V> V dict_get(FabDict<K,V>* d, K key)
{
    int e = dict_find(d, key);
    return e >= 0 ? d->vals[e] : dict_zero<V>();
}

template<class K, class V> V dict_get_or(FabDict<K,V>* d, K key, V def)
{
    int e = dict_find(d, key);
    return e >= 0 ? d->vals[e] : def;
}

template<class K, class V> K dict_key_at(FabDict<K,V>* d, int i)
{
    if (i < 0 || i >= d->len) return dict_zero<K>();
    return d->keys[i];
}

template<class K, class V> V dict_value_at(FabDict<K,V>* d, int i)
{
    if (i < 0 || i >= d->len) return dict_zero<V>();
    return d->vals[i];
}

template<class K, class V> bool dict_has_value(FabDict<K,V>* d, V val)
{
    for (int i = 0; i < d->len; ++i)
        if (dict_eq_val(d->vals[i], val)) return true;
    return false;
}

// Remove (strings are not freed, like everywhere in the lib)
template<class K, class V> bool dict_remove(FabDict<K,V>* d, K key)
{
    int e = dict_find(d, key);
    if (e < 0) return false;

    memmove(d->keys + e, d->keys + e + 1, (d->len - e - 1) * sizeof(K));
    memmove(d->vals + e, d->vals + e + 1, (d->len - e - 1) * sizeof(V));
    d->len--;

    dict_reindex(d);
    return true;
}

template<class K, class V> V dict_pop(FabDict<K,V>* d, K key)
{
    int e = dict_find(d, key);
    if (e < 0) return dict_zero<V>();

    V v = d->vals[e];
    dict_remove(d, key);
    return v;
}

template<class K, class V> void dict_clear(FabDict<K,V>* d)
{
    d->len = 0;
    if (d->icap) memset(d->index, 0xFF, d->icap * sizeof(int));
}

// Copy, merge
template<class K, class V> FabDict<K,V>* dict_copy(FabDict<K,V>* d)
{
    auto* r = dict_new<K,V>();
    for (int i = 0; i < d->len; ++i)
        dict_set(r, d->keys[i], d->vals[i]);
    return r;
}

template<class K, class V> void dict_merge(FabDict<K,V>* d, FabDict<K,V>* o)
{
    for (int i = 0; i < o->len; ++i)
        dict_set(d, o->keys[i], o->vals[i]);
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
  extern "C" void* fab_dict_new_##KN##_##VN()                       { return dict_new<K,V>(); } \
  extern "C" void  fab_dict_set_##KN##_##VN(void* d, K k, V v)      { dict_set<K,V>((FabDict<K,V>*)d, k, dict_norm(v)); } \
  extern "C" int   fab_dict_length_##KN##_##VN(void* d)             { return ((FabDict<K,V>*)d)->len; } \
  extern "C" bool  fab_dict_contains_##KN##_##VN(void* d, K k)      { return dict_find<K,V>((FabDict<K,V>*)d, k) >= 0; } \
  extern "C" void  fab_dict_print_##KN##_##VN(void* d)              { dict_print<K,V>((FabDict<K,V>*)d); } \
  extern "C" V     fab_dict_get_##KN##_##VN(void* d, K k)           { return dict_get<K,V>((FabDict<K,V>*)d, k); } \
  extern "C" V     fab_dict_get_or_##KN##_##VN(void* d, K k, V def) { return dict_get_or<K,V>((FabDict<K,V>*)d, k, dict_norm(def)); } \
  extern "C" bool  fab_dict_empty_##KN##_##VN(void* d)              { return ((FabDict<K,V>*)d)->len == 0; } \
  extern "C" int   fab_dict_capacity_##KN##_##VN(void* d)           { return ((FabDict<K,V>*)d)->cap; } \
  extern "C" bool  fab_dict_contains_value_##KN##_##VN(void* d, V v){ return dict_has_value<K,V>((FabDict<K,V>*)d, dict_norm(v)); } \
  extern "C" bool  fab_dict_remove_##KN##_##VN(void* d, K k)        { return dict_remove<K,V>((FabDict<K,V>*)d, k); } \
  extern "C" V     fab_dict_pop_##KN##_##VN(void* d, K k)           { return dict_pop<K,V>((FabDict<K,V>*)d, k); } \
  extern "C" void  fab_dict_clear_##KN##_##VN(void* d)              { dict_clear<K,V>((FabDict<K,V>*)d); } \
  extern "C" K     fab_dict_key_at_##KN##_##VN(void* d, int i)      { return dict_key_at<K,V>((FabDict<K,V>*)d, i); } \
  extern "C" V     fab_dict_value_at_##KN##_##VN(void* d, int i)    { return dict_value_at<K,V>((FabDict<K,V>*)d, i); } \
  extern "C" void  fab_dict_merge_##KN##_##VN(void* d, void* o)     { dict_merge<K,V>((FabDict<K,V>*)d, (FabDict<K,V>*)o); } \
  extern "C" void* fab_dict_copy_##KN##_##VN(void* d)               { return dict_copy<K,V>((FabDict<K,V>*)d); } \
  extern "C" void* fab_dict_keys_##KN##_##VN(void* d) \
  { \
      auto* x = (FabDict<K,V>*)d; \
      void* l = fab_list_new_##KN(); \
      for (int i = 0; i < x->len; ++i) fab_list_addend_##KN(l, x->keys[i]); \
      return l; \
  } \
  extern "C" void* fab_dict_values_##KN##_##VN(void* d) \
  { \
      auto* x = (FabDict<K,V>*)d; \
      void* l = fab_list_new_##VN(); \
      for (int i = 0; i < x->len; ++i) fab_list_addend_##VN(l, x->vals[i]); \
      return l; \
  } \
  extern "C" void fab_dict_free_##KN##_##VN(void* d) { dict_free<K,V>((FabDict<K,V>*)d); }

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