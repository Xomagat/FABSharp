//
// Created by Xomagat on 26.09.2026.
//

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct StdlibFunctionInfo
{
    std::string symbol;
    std::string return_type;
    std::vector<std::string> arg_types;
};

inline std::unordered_map<std::string, StdlibFunctionInfo> stdlib_symbols;
inline std::unordered_set<std::string> loaded_libs;
inline std::unordered_set<std::string> loaded_fab_modules;