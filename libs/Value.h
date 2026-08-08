//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>

class Value
{
private:

public:
    virtual ~Value() = default;

    virtual double as_double() const = 0;
    virtual std::string as_string() const = 0;
};