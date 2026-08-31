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

    bool assign(const std::string& name, std::unique_ptr<Value> value)
    {
        auto it = variables.find(name);
        if (it != variables.end())
        {
            it->second.value = std::move(value);
            return true;
        }
        if (parent != nullptr)
            return parent->assign(name, std::move(value));
        return false;
    }

    Val* revolve(const std::string& name)
    {
        auto it = variables.find(name);
        if (it != variables.end())
            return &it->second;

        if (parent != nullptr)
            return parent->revolve(name);

        return nullptr;
    }

    std::string get_type(const std::string& name)
    {
        auto it = variables.find(name);
        return it->first;
    }
};
