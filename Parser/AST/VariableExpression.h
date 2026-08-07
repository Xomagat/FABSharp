//
// Created by Xomagat on 07.08.2026.
//

#include "UnaryExpression.h"

#include <string>

class VariableExpression : public Expression
{
private:
    std::string name;

public:
    explicit VariableExpression(std::string name) : name(name) {}

    double eval() const override
    {

    }

    std::string to_str() const override
    {
        return  name;
    }
};