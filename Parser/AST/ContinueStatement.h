//
// Created by Xomagat on 14.08.2026.
//

#pragma once
#include "Statement.h"

class ContinueStatement : public Statement
{
private:

public:
    explicit ContinueStatement() {}

    void execute(Environment &env) const override
    {
        throw ContinueStatement();
    }
};