//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <memory>

#include "Environment.h"
#include "Value.h"
#include "../Parser/AST/Statement.h"

class Function
{
private:
public:
    virtual ~Function() = default;

    virtual std::unique_ptr<Value> execute(Environment& env, std::vector<std::unique_ptr<Value>> args) const = 0;
};

class UserDefineFunction : public Function
{
private:
    std::vector<std::string> arg_names;
    std::shared_ptr<Statement> body;

public:
    explicit UserDefineFunction(std::vector<std::string> arg_names, std::shared_ptr<Statement> body) : arg_names(arg_names), body(std::move(body)) {}

    int get_names_size()
    {
        return arg_names.size();
    }

    std::string get_name_index(int index)
    {
        if (index < 0 || index >= get_names_size()) return "";
        return arg_names[index];
    }

    std::unique_ptr<Value> execute(Environment& env, std::vector<std::unique_ptr<Value>> args) const override
    {
        Environment local(&env);
        body->execute(local);
        return nullptr;
    }
};