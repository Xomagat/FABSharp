//
// Created by Xomagat on 07.08.2026.
//

#pragma once

class Statement
{
private:
public:
    virtual ~Statement() = default;

    virtual void execute() const = 0;
};