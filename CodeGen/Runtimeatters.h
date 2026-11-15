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
            "to_rad", "to_deg", "lerp", "band", "bor", "bxor", "bnot", "shl", "shr", "ushr", "rotl", "rotr",
            "popcount", "clz", "ctz", "bit_length", "parity", "bit_get", "bit_set",
            "bit_clear", "bit_toggle", "bit_extract", "bswap", "bit_reverse",
            "is_pow2", "next_pow2"};
        for (auto* b : bases)
            for (auto* suf : {"i", "l", "f", "d"})
                r.insert(std::string("fab_") + b + suf);
        r.insert("fab_logbased");
        return r;
    }();
    return s;
}

inline llvm::Value* rt_try_bits(llvm::IRBuilder<>& b, const std::string& sym,
                                const std::vector<llvm::Value*>& a)
{
    if (!sym.starts_with("fab_") || sym.size() < 6 || a.empty()) return nullptr;

    char suf = sym.back();
    if (suf != 'i' && suf != 'l') return nullptr;

    unsigned W = suf == 'i' ? 32 : 64;
    llvm::Type* ty = a[0]->getType();
    if (!ty->isIntegerTy(W)) return nullptr;

    std::string base = sym.substr(4, sym.size() - 5);
    auto* i32 = b.getInt32Ty();
    auto narrow = [&](llvm::Value* v) -> llvm::Value* {      // count functions return int
        return W == 64 ? b.CreateTrunc(v, i32) : v;
    };
    auto lead  = [&] { return b.CreateIntrinsic(llvm::Intrinsic::ctlz,  {ty}, {a[0], b.getFalse()}); };
    auto trail = [&] { return b.CreateIntrinsic(llvm::Intrinsic::cttz,  {ty}, {a[0], b.getFalse()}); };

    // one argument
    if (a.size() == 1)
    {
        if (base == "bnot")        return b.CreateNot(a[0]);
        if (base == "popcount")    return narrow(b.CreateUnaryIntrinsic(llvm::Intrinsic::ctpop, a[0]));
        if (base == "clz")         return narrow(lead());
        if (base == "ctz")         return narrow(trail());
        if (base == "bit_length")  return narrow(b.CreateSub(llvm::ConstantInt::get(ty, W), lead()));
        if (base == "bswap")       return b.CreateUnaryIntrinsic(llvm::Intrinsic::bswap, a[0]);
        if (base == "bit_reverse") return b.CreateUnaryIntrinsic(llvm::Intrinsic::bitreverse, a[0]);
        return nullptr;
    }
    if (a.size() != 2) return nullptr;

    // two arguments
    if (base == "band") return b.CreateAnd(a[0], a[1]);
    if (base == "bor")  return b.CreateOr(a[0], a[1]);
    if (base == "bxor") return b.CreateXor(a[0], a[1]);

    // second argument is always int: bring it to the width of the first
    llvm::Value* n = b.CreateSExtOrTrunc(a[1], ty);

    if (base == "rotl") return b.CreateIntrinsic(llvm::Intrinsic::fshl, {ty}, {a[0], a[0], n});
    if (base == "rotr") return b.CreateIntrinsic(llvm::Intrinsic::fshr, {ty}, {a[0], a[0], n});

    // One unsigned compare also catches negative n
    llvm::Value* inRange = b.CreateICmpULT(n, llvm::ConstantInt::get(ty, W));
    llvm::Value* zero    = llvm::ConstantInt::get(ty, 0);

    if (base == "shl")  return b.CreateSelect(inRange, b.CreateShl(a[0], n),  zero);
    if (base == "ushr") return b.CreateSelect(inRange, b.CreateLShr(a[0], n), zero);
    if (base == "shr")  return b.CreateAShr(a[0],
                              b.CreateSelect(inRange, n, llvm::ConstantInt::get(ty, W - 1)));
    return nullptr;
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
    if (auto* r = rt_try_bits(b, sym, a)) return r;

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