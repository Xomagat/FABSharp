//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "../../CodeGen/CodegenContext.h"
#include "../Diagnostics.h"

class Expression
{
public:
    SrcLoc loc;

    virtual ~Expression() = default;

    virtual llvm::Value* codegen(CodegenContext& context) const
    {
        throw std::runtime_error("Codegen not implemented for this expression!");
    }

    virtual llvm::Value* codegen_expected(CodegenContext& context, const std::string& expected) const
    {
        return codegen(context);
    }

    virtual bool is_null_literal() const { return false; }

    virtual std::string to_str() const = 0;

    llvm::Value* gen(CodegenContext& c) const
        { return guard(loc, [&]{ return codegen(c); }); }
    llvm::Value* gen_expected(CodegenContext& c, const std::string& e) const
        { return guard(loc, [&]{ return codegen_expected(c, e); }); }
};