//
// Created by Xomagat on 07.08.2026.
//

#include "Expression.h"

#include <string>

class NumberExpression : public Expression
{
private:
    double value;

public:
    explicit NumberExpression(double value) : value(value) {}

    double eval() const override
    {
        return value;
    }
    std::string to_str() const override
    {
        return std::to_string(value);
    }
};