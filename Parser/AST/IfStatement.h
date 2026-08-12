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
        bool result = condition->eval()->as_bool();

        if (result)
        {
            if_statement->execute();
        }
        else if (else_statement != nullptr)
        {
            else_statement->execute();
        }
    }
};

class BlockStatement : public Statement
{
private:
    std::vector<std::unique_ptr<Statement>> statements;

public:
    explicit BlockStatement(std::vector<std::unique_ptr<Statement>> statements) : statements(std::move(statements)) {}

    void execute() const override
    {
        for (auto& s : statements)
        {
            s->execute();
        }
    }
};