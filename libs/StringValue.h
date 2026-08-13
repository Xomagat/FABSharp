//
// Created by Xomagat on 10.08.2026.
//

#pragma once
#include <string>

#include "Value.h"

class StringValue : public Value
{
private:
    std::string text;

public:
    explicit StringValue(std::string text) : text(text) {}

    std::variant<double, int> as_number() const override
    {
        try
        {
            return std::stod(text);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("It cannot be converted from text to a number!");
        }
    }

    std::string as_string() const override
    {
        return text;
    }

    bool as_bool() const override
    {
        return !text.empty();
    }

    std::unique_ptr<Value> clone() const override
    {
        return std::make_unique<StringValue>(text);
    }
};