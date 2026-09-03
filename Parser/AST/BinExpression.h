//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <cmath>
#include <memory>
#include <string>
#include <variant>

#include "../../libs/Environment.h"
#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"
#include "../../libs/Value.h"
#include "Expression.h"

template <typename Op>
std::variant<char, short, int, long, long long, double, long double> apply_numeric(
    const std::variant<char, short, int, long, long long, double, long double>& a,
    const std::variant<char, short, int, long, long long, double, long double>& b,
    Op op)
{
    return std::visit([&](auto x, auto y) -> std::variant<char, short, int, long, long long, double, long double> {
        return op(x, y);
    }, a, b);
}

class BinExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr1, expr2;
    char op;

public:
    explicit BinExpression(char op, std::unique_ptr<Expression> expr1, std::unique_ptr<Expression> expr2) : op(op), expr1(std::move(expr1)), expr2(std::move(expr2))
    {

    }

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        std::unique_ptr<Value> value1 = expr1->eval(env);
        std::unique_ptr<Value> value2 = expr2->eval(env);

        if (auto s1 = dynamic_cast<StringValue*>(value1.get()))
        {
            std::string s2 = value2->as_string();

            switch (op)
            {
                case '+': return std::make_unique<StringValue>(s1->as_string() + s2);
                default: throw std::runtime_error("Unknown operation!");
            }
        }

        auto n1 = value1->as_number();
        auto n2 = value2->as_number();

        switch (op)
        {
            case '+': return std::make_unique<NumberValue>(apply_numeric(n1, n2, std::plus<>()));
            case '-': return std::make_unique<NumberValue>(apply_numeric(n1, n2, std::minus<>()));
            case '*': return std::make_unique<NumberValue>(apply_numeric(n1, n2, std::multiplies<>()));
            case '/': return std::make_unique<NumberValue>(apply_numeric(n1, n2, [](auto x, auto y) {
                if (y == 0)
                    throw std::runtime_error("Division by zero!");
                return x / y;
            }));
            case '^': return std::make_unique<NumberValue>(apply_numeric(n1, n2, [](auto x, auto y) { return std::pow(x, y); }));
            default: throw std::runtime_error("Unknown operation!");
        }
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};