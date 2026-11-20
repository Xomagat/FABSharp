//
// Created by Xomagat on 14.08.2026.
//

#pragma once
#include "Statement.h"

class BreakStatement : public Statement
{
private:

public:
    void codegen(CodegenContext &context) const override
    {
        if (context.loop_stack.empty())
            throw std::runtime_error("'break' outside of a loop!");

        emit_unwind_to(context, context.loop_stack.back().try_depth);
        context.builder.CreateBr(context.loop_stack.back().breakTarget);
    }
};