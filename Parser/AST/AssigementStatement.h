//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "Statement.h"
#include "Expression.h"

#include "../../libs/Variables.h"
#include "../../libs/Value.h"

#include <memory>
#include <string>

class AssigementStatement : public Statement
{
private:
    std::string type, name;
    std::unique_ptr<Expression> expression;

public:
    explicit AssigementStatement(std::string type, std::string name, std::unique_ptr<Expression> expr) : type(type), name(name), expression(std::move(expr)) {}

    void execute() const override
    {
        std::unique_ptr<Value> result = expression->eval();
        Variables::set(type, name, std::move(result));
    }
};
