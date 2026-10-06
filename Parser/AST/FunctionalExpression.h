//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <string>
#include <vector>

#include "AssigementStatement.h"

#include "Expression.h"
#include "../../libs/STDLibInfo.h"

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
                                     const std::vector<llvm::Value*>& vals)
    {
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

        return ctx.builder.CreateCall(callee, coerced);
    }

    static llvm::Value* call(CodegenContext& context, const std::string& name,
                             std::vector<llvm::Value*> args_values)
    {
        std::vector<std::string> arg_type_names;
        for (auto* v : args_values)
            arg_type_names.push_back(llvm_to_type(v->getType()));

        auto key = mangle_name(name, arg_type_names);

        auto it = context.functions.find(key);
        if (it != context.functions.end())
            return context.builder.CreateCall(it->second, args_values);

        auto libIt = stdlib_symbols.find(key);
        if (libIt != stdlib_symbols.end())
        {
            std::vector<llvm::Type*> param_types;
            for (auto& t : libIt->second.arg_types)
                param_types.push_back(type_to_llvm(t, context.builder));

            auto fnType = llvm::FunctionType::get(
                type_to_llvm(libIt->second.return_type, context.builder), param_types, false);
            auto fn = context.module.getOrInsertFunction(libIt->second.symbol, fnType);

            std::vector<llvm::Value*> coerced;
            for (size_t i = 0; i < args_values.size(); i++)
                coerced.push_back(coerceToType(args_values[i], param_types[i], context.builder));

            return context.builder.CreateCall(fn, coerced);
        }

        throw std::runtime_error("Unknown function or overload for codegen: " + name);
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
