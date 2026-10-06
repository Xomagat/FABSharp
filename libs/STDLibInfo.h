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
inline std::unordered_map<std::string, StdlibFunctionInfo> stdlib_methods;

inline std::unordered_set<std::string> loaded_libs;
inline std::unordered_set<std::string> loaded_fab_modules;

inline std::string mangle_name(const std::string& name, const std::vector<std::string>& arg_types)
{
    std::string key = name + "#" + std::to_string(arg_types.size());
    for (auto& t : arg_types)
        key += "_" + t;
    return key;
}

inline std::string mangle_method(const std::string& type, const std::string& name,
                                 const std::vector<std::string>& args_without_receiver)
{
    return type + "::" + mangle_name(name, args_without_receiver);
}