//
// Created by Xomagat on 14.08.2026.
//

#pragma once
#include "Statement.h"
#include "BreakStatement.h"

class ContinueStatement : public Statement, public ControlFlowSignal
{
private:

public:
    explicit ContinueStatement() : ControlFlowSignal("'continue' outside of a loop!") {}

    void execute(Environment &env) const override
    {
        throw ContinueStatement();
    }

    void codegen(CodegenContext &context) const override
    {
        if (context.loop_stack.empty())
            std::runtime_error("'continue' outside of a loop!");

        context.builder.CreateBr(context.loop_stack.back().continueTarget);
    }
};