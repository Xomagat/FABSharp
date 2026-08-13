//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Value.h"

struct Val
{
    std::string type;
    std::unique_ptr<Value> value;
};

class Environment
{
private:
    std::unordered_map<std::string, Val> variables;
    Environment* parent;

public:
    explicit Environment(Environment* parent = nullptr) : parent(parent) {}

    void define(std::string type, std::string name, std::unique_ptr<Value> value)
    {
        variables[name] = {type, std::move(value)};
    }

    void define(std::string name, std::unique_ptr<Value> value)
    {
        auto it = variables.find(name);
        variables[name] = {it->first, std::move(value)};
    }

    Val* revolve(std::string name)
    {
        auto it = variables.find(name);
        if (it != variables.end())
            return &it->second;

        if (parent != nullptr)
            return parent->revolve(name);

        return nullptr;
    }
};
