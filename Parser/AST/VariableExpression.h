//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "Expression.h"

class VariableExpression : public Expression
{
private:
    std::string name;

public:
    explicit VariableExpression(std::string name) : name(name) {}

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