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
#include "../../libs/NullValue.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

inline std::unordered_map<std::string, std::function<std::unique_ptr<Value>(Value*)>> coercers = {
    {"int", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        int i = std::visit([](auto x) { return static_cast<int>(x); }, n->as_number());
        return std::make_unique<NumberValue>(i);
    }},
    {"double", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        double d = std::visit([](auto x) { return static_cast<double>(x); }, n->as_number());
        return std::make_unique<NumberValue>(d);
    }},
    {"float", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        float f = std::visit([](auto x) { return static_cast<float>(x); }, n->as_number());
        return std::make_unique<NumberValue>(f);
    }},
    {"short", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        short sh = std::visit([](auto x) { return static_cast<short>(x); }, n->as_number());
        return std::make_unique<NumberValue>(sh);
    }},
    {"long", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        long l = std::visit([](auto x) { return static_cast<long>(x); }, n->as_number());
        return std::make_unique<NumberValue>(l);
    }},
    {"byte", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        char ch = std::visit([](auto x) { return static_cast<char>(x); }, n->as_number());
        return std::make_unique<NumberValue>(ch);
    }},
    {"bool", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<BooleanValue*>(v);
        if (!n) return nullptr;
        bool b = n->as_bool();
        return std::make_unique<BooleanValue>(b);
    }},
    {"string", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<StringValue*>(v);
        if (!n) return nullptr;
        std::string str = n->as_string();
        return std::make_unique<StringValue>(str);
    }},
};

inline bool match_type(const std::string& type, Value* expr)
{
    if (dynamic_cast<NullValue*>(expr) != nullptr)
        return true;

    std::unordered_map<std::string, bool> match = {
        {"string", dynamic_cast<StringValue*>(expr) != nullptr},
        {"bool",   dynamic_cast<BooleanValue*>(expr) != nullptr},
        {"int",    dynamic_cast<NumberValue*>(expr) != nullptr},
        {"double", dynamic_cast<NumberValue*>(expr) != nullptr},
        {"float",  dynamic_cast<NumberValue*>(expr) != nullptr},
        {"short",  dynamic_cast<NumberValue*>(expr) != nullptr},
        {"long",   dynamic_cast<NumberValue*>(expr) != nullptr},
        {"byte",   dynamic_cast<NumberValue*>(expr) != nullptr},
    };

    return match.at(type);
}

class AssigementStatement : public Statement
{
private:
    std::string type, name;
    std::unique_ptr<Expression> expression;

public:
    explicit AssigementStatement(std::string type, std::string name, std::unique_ptr<Expression> expr) : type(std::move(type)), name(std::move(name)), expression(std::move(expr)) {
    }

    void execute(Environment& env) const override
    {
        std::unique_ptr<Value> result;

        if (expression)
        {
            result = expression->eval(env);
            if (!type.empty() && !match_type(type, result.get()))
                throw std::runtime_error("Uncorrected expression type!");
        }
        else
        {
            result = std::make_unique<NullValue>();
        }

        if (type.empty())
        {
            if (!env.assign(name, std::move(result)))
                throw std::runtime_error("Variable" + name + " not found!");
            return;
        }

        if (!match_type(type, result.get()))
            throw std::runtime_error("Uncorrected expression type!");

        env.define(type, name, std::move(coercers.at(type)(result.get())));
    }
};