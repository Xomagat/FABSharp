//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <string>
#include <vector>

#include "Expression.h"
#include "../../libs/Functions.h"

class FunctionalExpression : public Expression
{
private:
    std::string name;
    std::vector<std::unique_ptr<Expression>> args;

public:
    explicit FunctionalExpression(std::string name) : name(name) {}

    explicit FunctionalExpression(std::string name, std::vector<std::unique_ptr<Expression>> args) : name(name), args(std::move(args)) {}

    void add_arg(std::unique_ptr<Expression> arg)
    {
        args.push_back(std::move(arg));
    }

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        std::vector<std::unique_ptr<Value>> values;

        for (const auto& arg : args)
        {
            values.push_back(arg->eval(env));
        }

        Function* function = Functions::get(name);

        if (auto f = dynamic_cast<UserDefineFunction*>(function))
        {
            std::unique_ptr<UserDefineFunction> user_func = std::make_unique<UserDefineFunction>(*f);
            if (args.size() != user_func->get_names_size()) throw std::runtime_error("Argument count mismatch!");

            Environment local(&env);

            for (int i = 0; i < user_func->get_names_size(); i++)
                local.define("", user_func->get_name_index(i), std::move(values[i]));

            std::unique_ptr<Value> result = user_func->execute(local, {});
            return result;
        }

        return function->execute(env, std::move(values));
    }

    std::string to_str() const override
    {

    }
};
