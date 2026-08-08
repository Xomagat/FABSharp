//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "Statement.h"
#include "Expression.h"

#include "../../libs/Variables.h"
#include "../../libs/Value.h"

#include <string>
#include <memory>

class AssigementStatement : public Statement
{
private:
    std::string variable;
    std::unique_ptr<Expression> expression;

public:
    explicit AssigementStatement(std::string var, std::unique_ptr<Expression> expr) : variable(var), expression(std::move(expr)) {}

    void execute() const override
    {
        std::unique_ptr<Value> result = expression->eval();
        Variables::set(variable, std::move(result));
    }
};
