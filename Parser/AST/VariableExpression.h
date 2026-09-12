//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "../../libs/Environment.h"
#include "../../libs/Value.h"
#include "Expression.h"

class VariableExpression : public Expression
{
private:
    std::string name;

public:
    explicit VariableExpression(std::string name) : name(name) {}

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        const Val* val = env.revolve(name);

        if (!val)
            throw std::runtime_error("Variable {" + name + "} not found!");

        return val->value->clone();
    }

    llvm::Value *codegen(CodegenContext &context) const override
    {
        auto it = context.variables.find(name);
        if (it == context.variables.end())
            throw std::runtime_error("Variable {" + name + "} not found!");

        llvm::AllocaInst* alloc = it->second;
        return context.builder.CreateLoad(alloc->getAllocatedType(), alloc, name);
    }

    std::string to_str() const override
    {
        return name;
    }
};