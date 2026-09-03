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
    std::variant<char, short, int, long, long long, double, long double> value;

public:
    explicit NumberValue(std::variant<char, short, int, long, long long, double, long double> val) : value(val) {}

    std::variant<char, short, int, long, long long, double, long double> as_number() const override
    {
        return value;
    }

    std::string as_string() const override
    {
        return std::visit([](auto& v) { return std::to_string(v); }, value);
    }

    bool as_bool() const override
    {
        return std::visit([](auto v){ return v != 0; }, value); 
    }

    std::unique_ptr<Value> clone() const override
    {
        return std::make_unique<NumberValue>(value);
    }
};