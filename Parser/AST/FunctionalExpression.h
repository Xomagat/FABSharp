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

    llvm::Value *codegen(CodegenContext &context) const override
    {
        std::vector<llvm::Value*> args_values;
        for (auto& arg : args)
            args_values.push_back(arg->codegen(context));

        std::vector<std::string> arg_type_names;
        for (auto* v : args_values)
            arg_type_names.push_back(llvm_to_type(v->getType()));

        auto it = context.functions.find(mangle_name(name, arg_type_names));
        if (it != context.functions.end())
            return context.builder.CreateCall(it->second, args_values);

        auto libIt = stdlib_symbols.find(mangle_name(name, arg_type_names));
        if (libIt != stdlib_symbols.end())
        {
            std::vector<llvm::Type*> param_types;
            for (auto& t : libIt->second.arg_types)
                param_types.push_back(type_to_llvm(t, context.builder));

            auto fnType = llvm::FunctionType::get(
                type_to_llvm(libIt->second.return_type, context.builder), param_types, false);
            auto fn = context.module.getOrInsertFunction(libIt->second.symbol, fnType);

            std::vector<llvm::Value*> coerced_args;
            for (size_t i = 0; i < args_values.size(); i++)
                coerced_args.push_back(coerceToType(args_values[i], param_types[i], context.builder));

            return context.builder.CreateCall(fn, coerced_args);
        }

        throw std::runtime_error("Unknown function or overload for codegen: " + name);
    }

    std::string to_str() const override
    {
        return "define " + name + "(args count: " + std::to_string(args.size()) + ")";
    }
};
