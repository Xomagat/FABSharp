//
// Created by Xomagat on 07.08.2026.
//

#include "NumberExpression.h"

#include <memory>
#include <string>
#include <cmath>

class BinExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr1, expr2;
    char op;

public:
    explicit BinExpression(char op, std::unique_ptr<Expression> expr1, std::unique_ptr<Expression> expr2) : op(op), expr1(std::move(expr1)), expr2(std::move(expr2))
    {

    }

    double eval() const override
    {
        switch (op)
        {
            case '+': return expr1->eval() + expr2->eval();
            case '-': return expr1->eval() - expr2->eval();
            case '*': return expr1->eval() * expr2->eval();
            case '/': return expr1->eval() / expr2->eval();
            case '^': return std::pow(expr1->eval(), expr2->eval());
            default: throw std::runtime_error("Unknown operation!");
        }
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};