//
// Created by Xomagat on 07.08.2026.
//

#include "BinExpression.h"

#include <memory>
#include <string>

class UnaryExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr;
    char op;

public:
    explicit UnaryExpression(char op, std::unique_ptr<Expression> expr) : op(op), expr(std::move(expr)) {}

    double eval() const override
    {
        switch (op)
        {
            case '-': return -expr->eval();
            case '+': return expr->eval();
        }
    }

    std::string to_str() const override
    {
        return op + expr->to_str();
    }
};