//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <map>
#include <memory>

#include "Value.h"

class Variables
{
private:
    inline static std::map<std::string, std::unique_ptr<Value>> variables;

public:
    static bool is_exist(std::string name)
    {
        return variables.contains(name);
    }

    static std::unique_ptr<Value> get(std::string name)
    {
        if (!is_exist(name)) return 0;
        return std::move(variables[name]);
    }

    static void set(std::string name, std::unique_ptr<Value> value)
    {
        variables[name] = std::move(value);
    }
};
