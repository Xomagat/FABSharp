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
    std::vector<std::string> defaults;
};

struct StdlibVarInfo
{
    std::string symbol;
    std::string type;
    std::string value;
    bool is_const;
    bool is_getter = false;
};

inline std::unordered_set<std::string> class_names;

inline std::unordered_map<std::string, StdlibVarInfo> stdlib_vars;

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

struct LibTypeInfo
{
    std::string name;
    std::string parent;
    std::string free_symbol;
};

inline std::unordered_map<std::string, LibTypeInfo> lib_types;

inline bool is_lib_type(const std::string& t) { return lib_types.contains(t); }

inline bool lib_is_a(std::string from, const std::string& to)
{
    while (lib_types.contains(from))
    {
        if (from == to) return true;
        from = lib_types.at(from).parent;
    }
    return false;
}

inline std::string lib_free_symbol(std::string t)
{
    while (lib_types.contains(t))
    {
        auto& i = lib_types.at(t);
        if (!i.free_symbol.empty()) return i.free_symbol;
        t = i.parent;
    }
    return "";
}

inline const StdlibFunctionInfo* find_lib_method(const std::string& type, const std::string& name,
                                                 const std::vector<std::string>& tail)
{
    for (std::string t = type; !t.empty(); )
    {
        auto it = stdlib_methods.find(mangle_method(t, name, tail));
        if (it != stdlib_methods.end()) return &it->second;

        auto lt = lib_types.find(t);
        if (lt == lib_types.end()) break;
        t = lt->second.parent;
    }
    return nullptr;
}