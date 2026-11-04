//
// Created by Xomagat on 03.11.2026
//

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"

inline const std::unordered_set<std::string>& rt_pure_set()
{
    static const std::unordered_set<std::string> s = []
    {
        std::unordered_set<std::string> r;
        const char* bases[] = {
            "abs", "fabs", "pow", "sqrt", "cbrt", "hypot", "ceil", "floor", "round", "trunc",
            "fmod", "mod", "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh",
            "tanh", "exp", "log", "log2", "log10", "min", "max", "clamp", "sign", "gcd", "lcm",
            "factorial", "is_prime", "is_nan", "is_inf", "is_finite", "inf", "nan",
            "to_rad", "to_deg", "lerp"};
        for (auto* b : bases)
            for (auto* suf : {"i", "l", "f", "d"})
                r.insert(std::string("fab_") + b + suf);
        r.insert("fab_logbased");
        return r;
    }();
    return s;
}

inline bool rt_is_readonly(const std::string& n)
{
    static const std::unordered_set<std::string> exact = {
        "fab_length", "fab_empty", "fab_findc", "fab_finds", "fab_contains", "fab_compare",
        "fab_first", "fab_last", "fab_start_with", "fab_ends_with", "fab_count", "fab_at",
        "fab_rfinds", "fab_equals_ignore_case", "fab_is_blank", "fab_is_numeric"};
    if (exact.contains(n)) return true;

    static const char* prefixes[] = {
        "fab_list_length_", "fab_list_empty_", "fab_list_capacity_", "fab_list_get_",
        "fab_list_first_", "fab_list_last_", "fab_list_contains_", "fab_list_index_of_",
        "fab_list_count_", "fab_list_min_", "fab_list_max_",
        "fab_dict_get_", "fab_dict_contains_", "fab_dict_length_", "fab_dict_empty_",
        "fab_dict_capacity_", "fab_dict_key_at_", "fab_dict_value_at_"};
    for (auto* p : prefixes)
        if (n.starts_with(p)) return true;
    return false;
}

inline void rt_apply_attrs(llvm::Function* f)
{
    const std::string n = f->getName().str();
    if (!n.starts_with("fab_")) return;

    f->setDoesNotThrow();
    if (rt_pure_set().contains(n))
    {
        f->setDoesNotAccessMemory();
        f->setWillReturn();
    }
    else if (rt_is_readonly(n))
    {
        f->setOnlyReadsMemory();
        f->setWillReturn();
    }
}

inline llvm::Value* rt_try_intrinsic(llvm::IRBuilder<>& b, const std::string& sym,
                                     const std::vector<llvm::Value*>& a)
{
    static const std::unordered_map<std::string, llvm::Intrinsic::ID> unary = {
        {"sqrt",  llvm::Intrinsic::sqrt},  {"fabs",  llvm::Intrinsic::fabs},
        {"floor", llvm::Intrinsic::floor}, {"ceil",  llvm::Intrinsic::ceil},
        {"trunc", llvm::Intrinsic::trunc}, {"sin",   llvm::Intrinsic::sin},
        {"cos",   llvm::Intrinsic::cos},   {"exp",   llvm::Intrinsic::exp},
        {"log",   llvm::Intrinsic::log},   {"log2",  llvm::Intrinsic::log2},
        {"log10", llvm::Intrinsic::log10},
    };

    if (!sym.starts_with("fab_") || sym.size() < 6) return nullptr;
    char suf = sym.back();
    if (suf != 'd' && suf != 'f') return nullptr;
    for (auto* v : a)
        if (!v->getType()->isFloatTy() && !v->getType()->isDoubleTy()) return nullptr;

    std::string base = sym.substr(4, sym.size() - 5);
    if (a.size() == 1)
    {
        auto it = unary.find(base);
        if (it != unary.end()) return b.CreateUnaryIntrinsic(it->second, a[0]);
    }
    if (a.size() == 2 && base == "pow")
        return b.CreateBinaryIntrinsic(llvm::Intrinsic::pow, a[0], a[1]);
    return nullptr;
}