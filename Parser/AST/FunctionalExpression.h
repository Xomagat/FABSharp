//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <string>
#include <vector>

#include "AssigementStatement.h"

#include "Expression.h"
#include "../../libs/STDLibInfo.h"
#include "../../libs/Functions.h"

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

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        std::vector<std::unique_ptr<Value>> values;

        for (const auto& arg : args)
        {
            values.push_back(arg->eval(env));
        }

        Function* function = Functions::get(name);

        if (auto f = dynamic_cast<UserDefineFunction*>(function))
        {
            if (args.size() != f->get_names_size()) throw std::runtime_error("Argument count mismatch!");

            Environment local(&env);

            for (int i = 0; i < f->get_names_size(); i++)
            {
                if (!match_type(f->get_type_index(i), values[i].get()))
                    throw std::runtime_error("Uncorrected expression type!");

                local.define(f->get_type_index(i), f->get_name_index(i), std::move(values[i]));
            }

            std::unique_ptr<Value> result = f->execute(local, {});
            if (!result)
                throw std::runtime_error("Function '" + name + "' did not return a value!");
            return result;
        }

        return function->execute(env, std::move(values));
    }

    llvm::Value *codegen(CodegenContext &context) const override
    {
        std::vector<llvm::Value*> args_values;
        for (auto& arg : args)
            args_values.push_back(arg->codegen(context));

        auto it = context.functions.find(name);
        if (it != context.functions.end())
        {
            return context.builder.CreateCall(it->second, args_values);
        }

        auto libIt = stdlib_symbols.find(name);
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
    }

    std::string to_str() const override
    {
        return "define " + name + "(args count: " + std::to_string(args.size()) + ")";
    }
};
