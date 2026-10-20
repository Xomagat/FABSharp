//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <string>
#include <vector>

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

inline const StdlibFunctionInfo* find_with_conversion(const std::string& name,
                                                      const std::vector<std::string>& actual)
{
    const std::string prefix = name + "#" + std::to_string(actual.size()) + "_";

    const StdlibFunctionInfo* best = nullptr;
    int  best_cost = INT_MAX;
    bool tie = false;

    for (auto& [key, info] : stdlib_symbols)
    {
        if (!key.starts_with(prefix) || info.arg_types.size() != actual.size()) continue;

        int total = 0;
        bool ok = true;
        for (size_t i = 0; i < actual.size() && ok; ++i)
        {
            int c = conversion_cost(actual[i], info.arg_types[i]);
            if (c < 0) ok = false; else total += c;
        }
        if (!ok) continue;

        if (total < best_cost) { best = &info; best_cost = total; tie = false; }
        else if (total == best_cost) tie = true;
    }

    if (tie) throw std::runtime_error("Ambiguous call to '" + name + "'");
    return best;
}

class FunctionalExpression : public Expression
{
private:
    std::string name;
    std::vector<std::unique_ptr<Expression>> args;

public:
    explicit FunctionalExpression(std::string name) : name(name) {}

    explicit FunctionalExpression(std::string name, std::vector<std::unique_ptr<Expression>> args) : name(name), args(std::move(args)) {}

    void add_arg(std::unique_ptr<Expression> arg)
    {
        args.push_back(std::move(arg));
    }

    static llvm::Value* emit_stdlib_call(CodegenContext& ctx, const StdlibFunctionInfo& info,
                                     const std::vector<llvm::Value*>& given)
    {
        std::vector<llvm::Value*> vals = given;
        for (size_t i = vals.size(); i < info.arg_types.size(); ++i)
        {
            if (i >= info.defaults.size() || info.defaults[i].empty())
                throw std::runtime_error("Missing argument for " + info.symbol);
            vals.push_back(emit_stdlib_const(ctx, {"", info.arg_types[i], info.defaults[i], true}));
        }

        std::vector<llvm::Type*> params;
        for (auto& t : info.arg_types)
            params.push_back(type_to_llvm(t, ctx.builder));

        if (params.size() != vals.size())
            throw std::runtime_error("Internal error: argument count mismatch for " + info.symbol);

        auto* fnType = llvm::FunctionType::get(type_to_llvm(info.return_type, ctx.builder), params, false);
        auto callee  = ctx.module.getOrInsertFunction(info.symbol, fnType);

        std::vector<llvm::Value*> coerced;
        for (size_t i = 0; i < vals.size(); ++i)
            coerced.push_back(coerceToType(vals[i], params[i], ctx.builder));

        llvm::Value* r = ctx.builder.CreateCall(callee, coerced);
        ctx.value_types[r] = info.return_type;
        return r;
    }

    static llvm::Value* call(CodegenContext& context, const std::string& name,
                             std::vector<llvm::Value*> args_values)
    {
        std::vector<std::string> arg_type_names;
        for (auto* v : args_values)
            arg_type_names.push_back(type_of(context, v));

        auto key = mangle_name(name, arg_type_names);

        auto it = context.functions.find(key);
        if (it != context.functions.end())
        {
            auto it = context.functions.find(key);
            if (it != context.functions.end())
            {
                auto vals = args_values;

                auto d = context.func_defaults.find(key);
                if (d != context.func_defaults.end())
                {
                    auto& fd = d->second;
                    for (size_t i = vals.size(); i < fd.arg_types.size(); ++i)
                        vals.push_back(fd.defaults[i]->codegen_expected(context, fd.arg_types[i]));
                }

                auto* fty = it->second->getFunctionType();
                for (size_t i = 0; i < vals.size(); ++i)
                    vals[i] = coerceToType(vals[i], fty->getParamType(i), context.builder);

                auto* r = context.builder.CreateCall(it->second, vals);
                auto& rt = context.func_ret[key];
                if (rt != "void") context.value_types[r] = rt;
                return r;
            }
        }

        auto libIt = stdlib_symbols.find(key);
        if (libIt != stdlib_symbols.end())
            return emit_stdlib_call(context, libIt->second, args_values);

        std::vector<std::string> widened = arg_type_names;
        bool changed = false;
        for (auto& t : widened)
            if (t == "int" || t == "short" || t == "long" || t == "float")
            { t = "double"; changed = true; }

        if (changed)
        {
            auto wIt = stdlib_symbols.find(mangle_name(name, widened));
            if (wIt != stdlib_symbols.end())
                return emit_stdlib_call(context, wIt->second, args_values);
        }

        std::string sig = name + "(";
        for (size_t i = 0; i < arg_type_names.size(); ++i)
            sig += (i ? ", " : "") + arg_type_names[i];
        sig += ")";

        if (auto* info = find_with_conversion(name, arg_type_names))
            return emit_stdlib_call(context, *info, args_values);

        throw std::runtime_error("Unknown function or overload for codegen: " + sig);
    }

    llvm::Value* codegen(CodegenContext& context) const override
    {
        std::vector<llvm::Value*> vals;
        for (auto& a : args) vals.push_back(a->codegen(context));
        return call(context, name, std::move(vals));
    }

    std::string to_str() const override
    {
        return "define " + name + "(args count: " + std::to_string(args.size()) + ")";
    }
};
