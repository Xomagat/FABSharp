//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

#include "Value.h"

class NumberValue : public Value
{
private:
    double value;

public:
    explicit NumberValue(double val) : value(val) {}

    double as_double() const override
    {
        return value;
    }

    std::string as_string() const override
    {
        return std::to_string(value);
    }
};