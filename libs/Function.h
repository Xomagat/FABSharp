//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include "Environment.h"
#include "Value.h"

class Fuctions
{
private:
public:
    virtual ~Fuctions() = default;

    virtual Value* execute(Environment* env, Value... args) const = 0;
};
