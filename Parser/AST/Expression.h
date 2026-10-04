//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "../../CodeGen/CodegenContext.h"

class Expression
{
public:
    virtual ~Expression() = default;

    virtual llvm::Value* codegen(CodegenContext& context) const
    {
        throw std::runtime_error("Codegen not implemented for this expression!");
    }

    virtual bool is_null_literal() const { return false; }

    virtual std::string to_str() const = 0;
};