//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "FunctionalExpression.h"
#include "Statement.h"

class FunctionStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit FunctionStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment &env) const override
    {
        expr->eval(env);
    }
};

class FunctionDefineStatement : public Statement
{
private:
    std::string name;
    std::vector<std::string> arg_names;
    std::vector<std::string> arg_types;
    std::shared_ptr<Statement> body;

public:
    explicit FunctionDefineStatement(std::string name, std::vector<std::string> arg_types, std::vector<std::string> arg_names, std::unique_ptr<Statement> body)
        : name(name), arg_types(arg_types), arg_names(arg_names), body(std::move(body)) {}

    void execute(Environment &env) const override
    {
        Functions::define(name, new UserDefineFunction(arg_types, arg_names, body));
    }
};