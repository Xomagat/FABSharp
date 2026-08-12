//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "../../libs/BooleanValue.h"
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
                return std::make_unique<BooleanValue>(s1->as_string() == s2);
            }
            if (op == "!=")
            {
                return std::make_unique<BooleanValue>(s1->as_string() != s2);
            }

            throw std::runtime_error("Unknown operation!");
        }

        auto n1 = value1->as_number();
        auto n2 = value2->as_number();

        std::unordered_map<std::string, bool> operators = {
            {"==", n1 == n2},
            {"!=", n1 != n2},
            {"<=", n1 <= n2},
            {">=", n1 >= n2},
            {"<", n1 < n2},
            {">", n1 > n2},
            {"&&", std::visit([](auto x) -> bool { return x != 0; }, n1) && std::visit([](auto x) -> bool { return x != 0; }, n2)},
            {"||", std::visit([](auto x) -> bool { return x != 0; }, n1) || std::visit([](auto x) -> bool { return x != 0; }, n2)},
        };

        return std::make_unique<BooleanValue>(operators[op]);

        throw std::runtime_error("Unknown operation!");
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};