//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>

#include "Expression.h"
#include "../../libs/Value.h"
#include "../../libs/NumberValue.h"

class UnaryExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr;
    char op;

public:
    explicit UnaryExpression(char op, std::unique_ptr<Expression> expr) : op(op), expr(std::move(expr)) {}

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        switch (op)
        {
        case '-': return std::make_unique<NumberValue>(std::visit([](auto a) -> std::variant<double, int> { return -a; }, expr->eval(env)->as_number()));
            case '+': return std::make_unique<NumberValue>(expr->eval(env)->as_number());
            default: throw std::runtime_error("Undefined behavior!");
        }
    }

    std::string to_str() const override
    {
        return op + expr->to_str();
    }
};