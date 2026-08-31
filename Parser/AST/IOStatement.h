//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <iostream>

#include "Expression.h"
#include "Statement.h"

#include "../../libs/Environment.h"

class WritelnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WritelnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment& env) const override
    {
        std::cout << expr->eval(env)->as_string() << std::endl;
    }
};

class WriteStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WriteStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment& env) const override
    {
        std::cout << expr->eval(env)->as_string();
    }
};

class InputInStatement : public Statement
{
private:
    std::string name;

public:
    explicit InputInStatement(std::string name) : name(name) {}

    void execute(Environment& env) const override
    {
        Val* target = env.revolve(name);
        if (!target)
            throw std::runtime_error("Variable {" + name + "} not found!");

        std::string input;
        std::getline(std::cin, input);

        auto raw = std::make_unique<StringValue>(input);

        auto it = coercers.find(target->type);
        std::unique_ptr<Value> converted =
            (it != coercers.end()) ? it->second(raw.get()) : nullptr;

        if (!converted)
            throw std::runtime_error("Cannot convert input to type " + target->type);

        env.assign(name, std::move(converted));
    }
};