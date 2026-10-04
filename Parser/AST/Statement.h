//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "../../CodeGen/CodegenContext.h"

class Statement
{
private:
public:
    virtual ~Statement() = default;

    virtual void codegen(CodegenContext& context) const {
        throw std::runtime_error("Codegen not implemented for this statement!");
    }
};