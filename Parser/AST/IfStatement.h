//
// Created by Xomagat on 10.08.2026.
//

#pragma once
#include <memory>

#include "Expression.h"
#include "Statement.h"

#include "../../libs/Environment.h"

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

    void execute(Environment& env) const override
    {
        bool result = condition->eval(env)->as_bool();

        if (result)
        {
            if_statement->execute(env);
        }
        else if (else_statement != nullptr)
        {
            else_statement->execute(env);
        }
    }
};

class BlockStatement : public Statement
{
private:
    std::vector<std::unique_ptr<Statement>> statements;

public:
    explicit BlockStatement(std::vector<std::unique_ptr<Statement>> statements) : statements(std::move(statements)) {}

    void execute(Environment& env) const override
    {
        Environment local(&env);
        for (auto& s : statements)
        {
            s->execute(local);
        }
    }
};