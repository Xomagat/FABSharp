//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <memory>

#include "../../libs/Value.h"

class Expression
{
public:
    virtual ~Expression() = default;

    virtual std::unique_ptr<Value> eval() const = 0;
    virtual std::string to_str() const = 0;
};