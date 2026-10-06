//
// Created by Xomagat on 05.10.2026.
//

#include "Expression.h"

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Expression.h"
#include "FunctionalExpression.h"

class MethodCallExpression : public Expression
{
private:
    std::string name;
    std::unique_ptr<Expression> receiver;
    std::vector<std::unique_ptr<Expression>> args;

public:
    explicit MethodCallExpression(std::string& name, std::unique_ptr<Expression> receiver) : name(name), receiver(std::move(receiver)) {}

    void add_arg(std::unique_ptr<Expression> arg) { args.push_back(std::move(arg)); }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* self = receiver->codegen(ctx);
        std::string recv_type = llvm_to_type(self->getType());

        std::vector<llvm::Value*> vals{self};
        std::vector<std::string> tail_types;
        for (auto& a : args)
        {
            vals.push_back(a->codegen(ctx));
            tail_types.push_back(llvm_to_type(vals.back()->getType()));
        }

        auto it = stdlib_methods.find(mangle_method(recv_type, name, tail_types));
        if (it == stdlib_methods.end())
            throw std::runtime_error("Type '" + recv_type + "' has no method '" + name + "' with these arguments");

        return FunctionalExpression::emit_stdlib_call(ctx, it->second, vals);
    }

    std::string to_str() const override
    {
        return receiver->to_str() + "." + name + "(...)";
    }
};
