//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <string>
#include <vector>

#include "AssigementStatement.h"

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
            if (args.size() != f->get_names_size()) throw std::runtime_error("Argument count mismatch!");

            Environment local(&env);

            for (int i = 0; i < f->get_names_size(); i++)
            {
                if (!match_type(f->get_type_index(i), values[i].get()))
                    throw std::runtime_error("Uncorrected expression type!");

                local.define(f->get_type_index(i), f->get_name_index(i), std::move(values[i]));
            }

            std::unique_ptr<Value> result = f->execute(local, {});
            if (!result)
                throw std::runtime_error("Function '" + name + "' did not return a value!");
            return result;
        }

        return function->execute(env, std::move(values));
    }

    std::string to_str() const override
    {
        return "define " + name + "(args count: " + std::to_string(args.size()) + ")";
    }
};
