//
// Created by Xomagat on 14.08.2026.
//

#pragma once
#include "Statement.h"

class ContinueStatement : public Statement
{
private:

public:
    void codegen(CodegenContext &context) const override
    {
        if (context.loop_stack.empty())
            throw std::runtime_error("'continue' outside of a loop!");

        context.builder.CreateBr(context.loop_stack.back().continueTarget);
    }
};