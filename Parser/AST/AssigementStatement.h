//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "Statement.h"
#include "Expression.h"

#include "../../libs/Environment.h"
#include "../../libs/Value.h"

#include "../../libs/BooleanValue.h"
#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"

#include <memory>
#include <string>
#include <unordered_map>

inline bool match_type(const std::string& type, Value* expr)
{
    std::unordered_map<std::string, bool> match = {
        {"string", dynamic_cast<StringValue*>(expr) != nullptr},
        {"bool",   dynamic_cast<BooleanValue*>(expr) != nullptr},
        {"int",    dynamic_cast<NumberValue*>(expr) != nullptr},
        {"double", dynamic_cast<NumberValue*>(expr) != nullptr},
    };

    return match.at(type);
}

class AssigementStatement : public Statement
{
private:
    std::string type, name;
    std::unique_ptr<Expression> expression;

public:
    explicit AssigementStatement(std::string type, std::string name, std::unique_ptr<Expression> expr) : type(std::move(type)), name(std::move(name)), expression(std::move(expr)) {}

    void execute(Environment& env) const override
    {
        std::unique_ptr<Value> result = expression->eval(env);

        if (type.empty())
        {
            env.define(name, std::move(result));
            return;
        }

        if (!match_type(type, result.get()))
            throw std::runtime_error("Uncorrected expression type");

        env.define(type, name, std::move(result));
    }
};