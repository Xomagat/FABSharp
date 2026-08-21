//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <memory>

#include "FunctionalExpression.h"
#include "Statement.h"

class FunctionStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit FunctionStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment &env) const override
    {
        expr->eval(env);
    }
};