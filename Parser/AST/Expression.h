//
// Created by Xomagat on 07.08.2026.
//

#include <string>

class Expression
{
public:
    virtual ~Expression() = default;

    virtual double eval() const = 0;
    virtual std::string to_str() const = 0;
};