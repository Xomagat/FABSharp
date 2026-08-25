//
// Created by Xomagat on 25.08.2026.
//

#pragma once
#include <memory>

#include "Expression.h"
#include "Statement.h"

#include "../../libs/Value.h"

class ReturnException
{
private:
    std::unique_ptr<Value> value;
public:
    explicit ReturnException(std::unique_ptr<Value> value) : value(std::move(value)) {}

    std::unique_ptr<Value> take_value()
    {
        return std::move(value);
    }
};

class ReturnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit ReturnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment &env) const override
    {
        throw ReturnException(expr->eval(env));
    }
};