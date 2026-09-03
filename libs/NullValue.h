//
// Created by Xomagat on 01.09.2026.
//

#pragma once
#include <memory>
#include <string>
#include <format>

#include "Value.h"

class NullValue : public Value
{
private:

public:
    explicit NullValue() {}

    std::variant<char, short, int, long, long long, double, long double> as_number() const override
    {
        return NULL;
    }

    std::string as_string() const override
    {
        return "null";
    }

    bool as_bool() const override
    {
        return NULL;
    }

    std::unique_ptr<Value> clone() const override
    {
        return std::make_unique<NullValue>();
    }
};