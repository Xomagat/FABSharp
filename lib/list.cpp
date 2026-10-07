//
// Created by Xomagat on 07.10.2026.
//

#define _NO_CRT_STDIO_INLINE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

template<class T> struct FabList { T* data; int len; int cap;
};

template<class T> FabList<T>* list_new()
{
    auto* l = (FabList<T>*)malloc(sizeof(FabList<T>));
    l->data = nullptr; l->len = 0; l->cap = 0;
    return l;
}

template<class T> void ensure_list(FabList<T>* l)
{
    if (l->len < l->cap) return;
    l->cap = l->cap ? l->cap * 2 : 4;
    l->data = (T*)realloc(l->data, l->cap * sizeof(T));
}

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

inline void print_elem(int v)         { printf("%d", v); }
inline void print_elem(long long v)   { printf("%lld", v); }
inline void print_elem(float v)       { printf("%f", (double)v); }
inline void print_elem(double v)      { printf("%f", v); }
inline void print_elem(const char* v) { printf("%s", v); }

template<class T> void list_print(FabList<T>* l, const char* name)
{
    printf("list<%s>[", name);
    for (int i = 0; i < l->len; ++i)
    {
        if (i) printf(", ");
        print_elem(l->data[i]);
    }
    printf("]");
}

#define FAB_LIST(NAME, T) \
    extern "C" void* fab_list_new_##NAME()                { return list_new<T>(); } \
    extern "C" void  fab_list_addend_##NAME(void* l, T v) { list_addend<T>((FabList<T>*)l, v); } \
    extern "C" void  fab_list_add_##NAME(void* l, T v)    { list_add<T>((FabList<T>*)l, v); } \
    extern "C" int   fab_list_length_##NAME(void* l)      { return ((FabList<T>*)l)->len; } \
    extern "C" void  fab_list_print_##NAME(void* l)       { list_print<T>((FabList<T>*)l, #NAME); }

FAB_LIST(int,    int)
FAB_LIST(long,   long long)
FAB_LIST(float,  float)
FAB_LIST(double, double)
FAB_LIST(string, const char*)