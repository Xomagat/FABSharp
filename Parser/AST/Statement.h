//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "../../CodeGen/CodegenContext.h"
#include "../Diagnostics.h"

class Statement
{
private:
public:
    SrcLoc loc;

    virtual ~Statement() = default;

    virtual void codegen(CodegenContext& context) const {
        throw std::runtime_error("Codegen not implemented for this statement!");
    }

    void gen(CodegenContext& c) const
        { guard(loc, [&]{ codegen(c); }); }
};