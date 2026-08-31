//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Value.h"
#include "Function.h"
#include "NumberValue.h"

class Functions
{
private:
    inline static std::unordered_map<std::string, Function*> functions = {
    };

public:
    static bool is_exist(std::string name)
    {
        std::vector<std::string> names;

        for (const auto& name : functions)
            names.push_back(name.first);

        return functions.contains(name);
    }

    static Function* get(std::string name)
    {
        if (!is_exist(name)) throw std::runtime_error("Unknown function: " + name + '!');
        return functions[name];
    }

    static void define(std::string name, Function* function)
    {
        functions[name] = function;
    }
};
