//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>

#include "../../libs/Environment.h"
#include "../../libs/Value.h"

class Expression
{
public:
    virtual ~Expression() = default;

    virtual std::unique_ptr<Value> eval(Environment& env) const = 0;
    virtual std::string to_str() const = 0;
};