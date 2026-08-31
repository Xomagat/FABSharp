//
// Created by Xomagat on 14.08.2026.
//

#pragma once
#include "Statement.h"

class ControlFlowSignal : public std::runtime_error
{
    public:
    explicit ControlFlowSignal(const std::string& what)
        : std::runtime_error(what) {}
};

class BreakStatement : public Statement, public ControlFlowSignal
{
private:

public:
    explicit BreakStatement() : ControlFlowSignal("'break' outside of a loop!") {}

    void execute(Environment &env) const override
    {
        throw BreakStatement();
    }
};