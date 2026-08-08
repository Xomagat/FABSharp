//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "Expression.h"
#include "../../libs/Variables.h"
#include "../../libs/Value.h"

class VariableExpression : public Expression
{
private:
    std::string name;

public:
    explicit VariableExpression(std::string name) : name(name) {}

    std::unique_ptr<Value> eval() const override
    {
        if (!Variables::is_exist(name))
            throw std::runtime_error("Variable {" + name + "} does not found!");

        return Variables::get(name);
    }

    std::string to_str() const override
    {
        return name;
    }
};