//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <variant>

#include "Value.h"

class NumberValue : public Value
{
private:
    std::variant<double, int> value;

public:
    explicit NumberValue(std::variant<double, int> val) : value(val) {}

    std::variant<double, int> as_number() const override
    {
        return value;
    }

    std::string as_string() const override
    {
        return std::visit([](auto& v) { return std::to_string(v); }, value);
    }

    bool as_bool() const override
    {
        if (value.valueless_by_exception() == 0) return false;
        return true;
    }

    std::unique_ptr<Value> clone() const override
    {
        return std::make_unique<NumberValue>(value);
    }
};