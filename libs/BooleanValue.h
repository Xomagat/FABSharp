//
// Created by Xomagat on 11.08.2026.
//

#pragma once
#include <string>
#include <format>

#include "Value.h"

class BooleanValue : public Value
{
private:
    bool value;

public:
    explicit BooleanValue(bool value) : value(value) {}

    std::variant<char, short, int, long, long long, double, long double> as_number() const override
    {
        return value == true || value == 1 ? 1 : 0;
    }

    std::string as_string() const override
    {
        return value == true || value == 1 ? "true" : "false";
    }

    bool as_bool() const override
    {
        return value;
    }

    std::unique_ptr<Value> clone() const override
    {
        return std::make_unique<BooleanValue>(value);
    }
};