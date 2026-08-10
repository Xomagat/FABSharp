//
// Created by Xomagat on 10.08.2026.
//

#pragma once
#include <memory>

#include "Expression.h"
#include "Statement.h"

class IfStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> if_statement, else_statement;

public:
    explicit IfStatement(std::unique_ptr<Expression> condition,
        std::unique_ptr<Statement> if_statement,
        std::unique_ptr<Statement> else_statement) : condition(std::move(condition)),
                                                     if_statement(std::move(if_statement)),
                                                     else_statement(std::move(else_statement))
    {

    }

    void execute() const override
    {
        double result = condition->eval()->as_double();

        if (result != 0)
        {
            if_statement->execute();
        }
        else if (else_statement != nullptr)
        {
            else_statement->execute();
        }
    }
};