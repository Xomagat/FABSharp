//
// Created by Xomagat on 25.11.2026.
//

#pragma once
#include <string>
#include <vector>
#include <unordered_set>

#include "FunctionalExpression.h"

inline llvm::Value* elem_size_i32(CodegenContext& ctx, const std::string& elem)
{
    auto& b = ctx.builder;
    auto* nullp = llvm::ConstantPointerNull::get(b.getPtrTy());
    auto* sz = b.CreatePtrToInt(b.CreateGEP(type_to_llvm(elem, b), nullp, {b.getInt32(1)}), b.getInt64Ty());
    return b.CreateTrunc(sz, b.getInt32Ty());
}

inline llvm::Value* emit_struct_list_method(CodegenContext& ctx, llvm::Value* list,
                                            const std::string& list_type, const std::string& name,
                                            const std::vector<llvm::Value*>& a)
{
    if (name == "length" || name == "empty" || name == "capacity" || name == "clear") return nullptr;

    auto& b = ctx.builder;
    const std::string elem = list_elem(list_type);
    llvm::Type* st = type_to_llvm(elem, b);
    llvm::Type* P = b.getPtrTy();
    llvm::Type* I = b.getInt32Ty();
    llvm::Type* V = b.getVoidTy();

    emit_null_check(ctx, list, "Null reference: method '" + name + "'");
    llvm::Value* esz = elem_size_i32(ctx, elem);

    auto call = [&](const char* fn, llvm::Type* ret, std::vector<llvm::Type*> pt, std::vector<llvm::Value*> av)
    {
        auto c = ctx.module.getOrInsertFunction(fn, llvm::FunctionType::get(ret, pt, false));
        return b.CreateCall(c, av);
    };
    auto argc = [&](size_t n)
    {
        if (a.size() != n)
            throw std::runtime_error("Method '" + name + "' takes " + std::to_string(n) + " argument(s)");
    };
    auto idx = [&](llvm::Value* v) { return coerceToType(v, I, b); };

    auto in_elem = [&](llvm::Value* v) -> llvm::Value*
    {
        std::string t = type_of(ctx, v);
        if (t != elem)
            throw std::runtime_error("list<" + elem + "> expects a '" + elem + "' value, got '" + t + "'");
        auto* tmp = entry_alloca(b, st, "slist.in");
        b.CreateStore(v, tmp);
        return tmp;
    };

    auto read = [&](const char* fn, std::vector<llvm::Type*> pt, std::vector<llvm::Value*> av) -> llvm::Value*
    {
        auto* out = entry_alloca(b, st, "slist.out");
        pt.push_back(P); av.push_back(out);
        pt.push_back(I); av.push_back(esz);
        call(fn, V, pt, av);
        llvm::Value* v = b.CreateLoad(st, out);
        ctx.value_types[v] = elem;
        return v;
    };
    auto list_result = [&](llvm::Value* r) { ctx.value_types[r] = list_type; return r; };

    if (name == "addend") { argc(1); return call("fab_slist_addend", V, {P, P, I}, {list, in_elem(a[0]), esz}); }
    if (name == "add")    { argc(1); return call("fab_slist_add",    V, {P, P, I}, {list, in_elem(a[0]), esz}); }
    if (name == "insert") { argc(2); return call("fab_slist_insert", V, {P, I, P, I}, {list, idx(a[0]), in_elem(a[1]), esz}); }
    if (name == "set")    { argc(2); return call("fab_slist_set",    V, {P, I, P, I}, {list, idx(a[0]), in_elem(a[1]), esz}); }

    if (name == "remove_at") { argc(1); return call("fab_slist_remove_at", V, {P, I, I}, {list, idx(a[0]), esz}); }
    if (name == "swap")      { argc(2); return call("fab_slist_swap", V, {P, I, I, I}, {list, idx(a[0]), idx(a[1]), esz}); }
    if (name == "reverse")   { argc(0); return call("fab_slist_reverse", V, {P, I}, {list, esz}); }
    if (name == "reserve")   { argc(1); return call("fab_slist_reserve", V, {P, I, I}, {list, idx(a[0]), esz}); }

    if (name == "extend")
    {
        argc(1);
        if (type_of(ctx, a[0]) != list_type)
            throw std::runtime_error("extend: expected " + list_type + ", got " + type_of(ctx, a[0]));
        emit_null_check(ctx, a[0], "Null reference: extend argument");
        return call("fab_slist_extend", V, {P, P, I}, {list, a[0], esz});
    }
    if (name == "copy")  { argc(0); return list_result(call("fab_slist_copy", P, {P, I}, {list, esz})); }
    if (name == "slice") { argc(2); return list_result(call("fab_slist_slice", P, {P, I, I, I}, {list, idx(a[0]), idx(a[1]), esz})); }

    if (name == "get")       { argc(1); return read("fab_slist_get", {P, I}, {list, idx(a[0])}); }
    if (name == "first")     { argc(0); return read("fab_slist_get", {P, I}, {list, b.getInt32(0)}); }
    if (name == "last")
    {
        argc(0);
        llvm::Value* len = b.CreateLoad(I, b.CreateGEP(b.getInt8Ty(), list, b.getInt64(8)));   // len лежит по смещению 8
        return read("fab_slist_get", {P, I}, {list, b.CreateSub(len, b.getInt32(1))});
    }
    if (name == "pop")       { argc(0); return read("fab_slist_pop", {P}, {list}); }
    if (name == "pop_first") { argc(0); return read("fab_slist_pop_first", {P}, {list}); }

    throw std::runtime_error("Method '" + name + "' is not available for list<" + elem + ">: structs have no ==, <, "
                             "so contains/index_of/count/remove/sort/min/max/sum/join are disabled");
}

inline void emit_list_addend(CodegenContext& ctx, const std::string& list_type, llvm::Value* list, llvm::Value* v)
{
    std::string elem = list_elem(list_type);
    if (is_struct(elem)) { emit_struct_list_method(ctx, list, list_type, "addend", {v}); return; }

    StdlibFunctionInfo info{"fab_list_addend_" + list_sym(elem), "void", {list_type, elem}};
    FunctionalExpression::emit_stdlib_call(ctx, info, {list, v});
}

inline void check_list_method(const std::string& list_type, const std::string& name)
{
    std::string e = list_elem(list_type);
    if (is_struct(e)) return;
    if (!(is_class(e) || is_container(e) || is_lib_type(e))) return;

    static const std::unordered_set<std::string> bad = {"sort", "sort_desc", "min", "max", "sum", "join"};
    if (bad.contains(name))
        throw std::runtime_error("Method '" + name + "' is not available for list<" + e + ">: objects have no order");
}