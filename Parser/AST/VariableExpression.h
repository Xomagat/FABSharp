//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "AssigementStatement.h"
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
        {
            std::string ft;
            if (auto* p = emit_this_field_ptr(context, name, ft))
            {
                auto* v = context.builder.CreateLoad(type_to_llvm(ft, context.builder), p, name);
                context.value_types[v] = ft;
                return v;
            }
            auto lib = stdlib_vars.find(name);
            if (lib == stdlib_vars.end())
                throw std::runtime_error("Variable {" + name + "} not found!");

            if (lib->second.is_const)
                return emit_stdlib_const(context, lib->second);

            auto* g = stdlib_global(context, lib->second);
            auto* v = context.builder.CreateLoad(g->getValueType(), g, name);
            context.value_types[v] = lib->second.type;
            return v;
        }

        llvm::AllocaInst* alloc = it->second;
        auto* v = context.builder.CreateLoad(alloc->getAllocatedType(), alloc, name);
        context.value_types[v] = context.var_types[name];
        return v;
    }

    std::string to_str() const override
    {
        return name;
    }
};