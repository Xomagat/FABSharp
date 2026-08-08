//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <iostream>

#include "Expression.h"
#include "Statement.h"

class WritelnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WritelnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute() const override
    {
        std::cout << expr->eval()->as_string() << std::endl;
    }
};

class WriteStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WriteStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute() const override
    {
        std::cout << expr->eval()->as_string();
    }
};