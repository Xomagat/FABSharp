//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "../../libs/Value.h"
#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"

#include "Expression.h"

class ValueExpression : public Expression
{
private:
    std::unique_ptr<Value> value;

public:
    explicit ValueExpression(double value)
    {
        this->value = std::make_unique<NumberValue>(value);
    }
    explicit ValueExpression(std::string value)
    {
        this->value = std::make_unique<StringValue>(value);
    }

    std::unique_ptr<Value> eval() const override
    {
        if (auto s = dynamic_cast<StringValue*>(value.get()))
            return std::make_unique<StringValue>(s->as_string());
        return std::make_unique<NumberValue>(value->as_double());
    }

    std::string to_str() const override
    {
        return value->as_string();
    }
};