//
// Created by Xomagat on 13.08.2026.
//

#pragma once
#include "Expression.h"
#include "Statement.h"

class WhileStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> while_statement;

public:
    explicit WhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> while_statement) : condition(std::move(condition)), while_statement(std::move(while_statement)) {}

    void execute(Environment &env) const override
    {
        while (condition->eval(env)->as_bool())
        {
            while_statement->execute(env);
        }
    }
};

class ForStatement : public Statement
{
private:
    std::unique_ptr<Statement> initialization;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> increment;
    std::unique_ptr<Statement> for_statement;

public:
    explicit ForStatement(std::unique_ptr<Statement> initialization, std::unique_ptr<Expression> condition,
        std::unique_ptr<Statement> increment, std::unique_ptr<Statement> for_statement) : initialization(std::move(initialization)),
                                                                                          condition(std::move(condition)),
                                                                                          increment(std::move(increment)),
                                                                                          for_statement(std::move(for_statement)) {}

    void execute(Environment &env) const override
    {
        for (initialization->execute(env); condition->eval(env)->as_bool(); increment->execute(env))
        {
            for_statement->execute(env);
        }
    }
};