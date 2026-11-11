//
// Created by Xomagat on 10.11.2026.
//

#pragma once
#include <climits>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdexcept>

#include "../../CodeGen/CodegenContext.h"

#include "AssigementStatement.h"
#include "Expression.h"
#include "../../libs/STDLibInfo.h"

inline int numeric_rank(const std::string& t)
{
    if (t == "short")  return 1;
    if (t == "int")    return 2;
    if (t == "long")   return 3;
    if (t == "float")  return 4;
    if (t == "double") return 5;
    if (t == "ldouble") return 6;
    return 0;
}

inline int conversion_cost(const std::string& from, const std::string& to)
{
    if (from == to) return 0;
    if (lib_is_a(from, to)) return 1;

    int a = numeric_rank(from), b = numeric_rank(to);
    if (!a || !b) return -1;
    if (a >= 4 && b <= 3) return -1;

    return std::abs(a - b);
}

struct NamedCand
{
    std::vector<std::string> types, names;
    std::vector<bool> has_default;
    std::function<llvm::Value*(CodegenContext&, size_t)> make_default;
    std::function<llvm::Value*(CodegenContext&, std::vector<llvm::Value*>&)> emit;
};

inline bool any_named(const std::vector<std::string>& n)
{
    for (auto& s : n) if (!s.empty()) return true;
    return false;
}

inline bool bind_named(const std::vector<std::string>& cn, size_t argc,
                       const NamedCand& c, std::vector<int>& slot)
{
    size_t n = c.types.size();
    if (argc > n) return false;
    slot.assign(n, -1);

    bool seen_named = false;
    for (size_t i = 0; i < argc; ++i)
    {
        if (cn[i].empty())
        {
            if (seen_named) return false;
            slot[i] = (int)i;
            continue;
        }
        seen_named = true;
        size_t p = 0;
        while (p < n && c.names[p] != cn[i]) ++p;
        if (p == n || slot[p] != -1) return false;
        slot[p] = (int)i;
    }
    for (size_t p = 0; p < n; ++p)
        if (slot[p] < 0 && !c.has_default[p]) return false;
    return true;
}

inline llvm::Value* emit_named_call(CodegenContext& ctx,
                                    const std::vector<std::unique_ptr<Expression>>& args,
                                    std::vector<std::string> cn,
                                    const std::vector<NamedCand>& cands,
                                    const std::string& what, bool soft = false)
{
    auto& b = ctx.builder;
    cn.resize(args.size());

    struct M { const NamedCand* c; std::vector<int> slot; };
    std::vector<M> ms;
    for (auto& c : cands)
    {
        std::vector<int> slot;
        if (bind_named(cn, args.size(), c, slot)) ms.push_back({&c, std::move(slot)});
    }
    if (ms.empty())
    {
        if (soft) return nullptr;
        throw std::runtime_error("No overload of '" + what + "' matches these arguments");
    }

    std::vector<llvm::Value*> pre(args.size(), nullptr);
    size_t pick = 0;
    if (ms.size() > 1)
    {
        for (size_t i = 0; i < args.size(); ++i)
            if (!args[i]->is_null_literal()) pre[i] = args[i]->codegen(ctx);

        int best = INT_MAX; bool tie = false;
        for (size_t m = 0; m < ms.size(); ++m)
        {
            int total = 0; bool ok = true;
            for (size_t p = 0; p < ms[m].slot.size() && ok; ++p)
            {
                int i = ms[m].slot[p];
                if (i < 0 || !pre[i]) continue;
                int c = conversion_cost(type_of(ctx, pre[i]), ms[m].c->types[p]);
                if (c < 0) ok = false; else total += c;
            }
            if (!ok) continue;
            if (total < best)       { best = total; pick = m; tie = false; }
            else if (total == best) tie = true;
        }
        if (best == INT_MAX) throw std::runtime_error("No overload of '" + what + "' accepts these argument types");
        if (tie)             throw std::runtime_error("Ambiguous call to '" + what + "'");
    }

    const NamedCand& c = *ms[pick].c;
    auto& slot = ms[pick].slot;
    size_t n = c.types.size();
    std::vector<llvm::Value*> vals(n, nullptr);

    for (size_t i = 0; i < args.size(); ++i)
    {
        size_t p = 0;
        while (slot[p] != (int)i) ++p;
        llvm::Type* lt = type_to_llvm(c.types[p], b);
        llvm::Value* v = pre[i] ? pre[i]
                       : args[i]->is_null_literal() ? llvm::Constant::getNullValue(lt)
                       : args[i]->codegen_expected(ctx, c.types[p]);
        vals[p] = coerceToType(v, lt, b);
    }
    for (size_t p = 0; p < n; ++p)
        if (!vals[p]) vals[p] = coerceToType(c.make_default(ctx, p), type_to_llvm(c.types[p], b), b);

    return c.emit(ctx, vals);
}

inline NamedCand sig_cand(const MethodInfo& mi)
{
    NamedCand c;
    c.types = mi.args;
    c.names = mi.names;  c.names.resize(mi.args.size());
    for (size_t p = 0; p < mi.args.size(); ++p)
        c.has_default.push_back(p < mi.defaults.size() && mi.defaults[p]);
    const MethodInfo* mp = &mi;
    c.make_default = [mp](CodegenContext& ctx, size_t p) {
        return mp->defaults[p]->codegen_expected(ctx, mp->args[p]);
    };
    return c;
}