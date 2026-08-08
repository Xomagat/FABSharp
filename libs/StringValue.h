//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <format>

#include "Value.h"


class StringValue : public Value
{
private:
    std::string text;

public:
    explicit StringValue(std::string text) : text(text) {}

    double as_double() const override
    {
        try
        {
            return std::stod(text);
        }
        catch (std::format_error)
        {
            throw std::runtime_error("It cannot be converted from text to a number!");
        }
    }

    std::string as_string() const override
    {
        return text;
    }
};