//
// Created by Xomagat on 13.08.2026.
//

#pragma once
#include "BreakStatement.h"
#include "ContinueStatement.h"

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
            try
            {
                while_statement->execute(env);
            }
            catch (const BreakStatement&)
            {
                break;
            }
            catch (const ContinueStatement&)
            {
                continue;
            }
        }
    }
};

class DoWhileStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> while_statement;

public:
    explicit DoWhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> while_statement) : condition(std::move(condition)), while_statement(std::move(while_statement)) {}

    void execute(Environment &env) const override
    {
        do
        {
            try
            {
                while_statement->execute(env);
            }
            catch (const BreakStatement&)
            {
                break;
            }
            catch (const ContinueStatement&)
            {
                continue;
            }
        } while (condition->eval(env)->as_bool());
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
            try
            {
                for_statement->execute(env);
            }
            catch (const BreakStatement&)
            {
                break;
            }
            catch (const ContinueStatement&)
            {
                continue;
            }
        }
    }
};