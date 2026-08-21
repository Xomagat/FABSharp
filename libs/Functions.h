//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <variant>

class Value
{
private:

public:
    virtual ~Value() = default;

    virtual std::variant<char, short, int, long, long long, double, long double> as_number() const = 0;
    virtual std::string as_string() const = 0;
    virtual bool as_bool() const = 0;

    virtual std::unique_ptr<Value> clone() const = 0;
};