//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <cmath>

#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"
#include "../../libs/Value.h"
#include "Expression.h"

class ConditionalExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr1, expr2;
    std::string op;

public:
    explicit ConditionalExpression(std::string op,
        std::unique_ptr<Expression> expr1,
        std::unique_ptr<Expression> expr2) : op(op), expr1(std::move(expr1)), expr2(std::move(expr2))
    {

    }

    std::unique_ptr<Value> eval() const override
    {
        std::unique_ptr<Value> value1 = expr1->eval();
        std::unique_ptr<Value> value2 = expr2->eval();

        if (auto s1 = dynamic_cast<StringValue*>(value1.get()))
        {
            std::string s2 = value2->as_string();

            if (op == "==")
            {
                return std::make_unique<NumberValue>(s1->as_string() == s2 ? 1 : 0);
            }

            throw std::runtime_error("Unknown operation!");
        }

        auto n1 = value1->as_double();
        auto n2 = value2->as_double();

        if (op == "==")
        {
            return std::make_unique<NumberValue>(n1 == n2 ? 1 : 0);
        }
        else if (op == ">=")
        {
            return std::make_unique<NumberValue>(n1 >= n2 ? 1 : 0);
        }
        else if (op == "<=")
        {
            return std::make_unique<NumberValue>(n1 <= n2 ? 1 : 0);
        }
        else if (op == ">")
        {
            return std::make_unique<NumberValue>(n1 > n2 ? 1 : 0);
        }
        else if (op == "<")
        {
            return std::make_unique<NumberValue>(n1 < n2 ? 1 : 0);
        }

        throw std::runtime_error("Unknown operation!");
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};