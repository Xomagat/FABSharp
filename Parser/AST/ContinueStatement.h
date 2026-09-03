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
    explicit ContinueStatement() : ControlFlowSignal("'break' outside of a loop!") {}

    void execute(Environment &env) const override
    {
        throw ContinueStatement();
    }
};