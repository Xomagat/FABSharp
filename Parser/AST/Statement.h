//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "../../libs/Environment.h"

class Statement
{
private:
public:
    virtual ~Statement() = default;

    virtual void execute(Environment& env) const = 0;
};