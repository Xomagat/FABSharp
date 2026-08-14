//
// Created by Xomagat on 14.08.2026.
//

#pragma once
#include "Statement.h"

class BreakStatement : public Statement
{
private:

public:
    explicit BreakStatement() {}

    void execute(Environment &env) const override
    {
        throw BreakStatement();
    }
};