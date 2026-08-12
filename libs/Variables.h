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

class Variables
{
private:
    inline static std::unordered_map<std::string, Val> variables;

public:
    static bool is_exist(std::string name)
    {
        return variables.contains(name);
    }

    static std::unique_ptr<Value> get(std::string name)
    {
        if (!is_exist(name))
            throw std::runtime_error("Variable {" + name + "} does not found!");

        auto& [type, val] = variables.at(name);
        return val->clone();
    }

    static void set(std::string type, std::string name, std::unique_ptr<Value> value)
    {
        variables[name] = {type, std::move(value)};
    }

    static std::string get_type(std::string name)
    {
        if (!is_exist(name))
            throw std::runtime_error("Variable {" + name + "} does not found!");

        auto& [type, val] = variables.at(name);
        return type;
    }
};
