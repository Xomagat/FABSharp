//
// Created by Xomagat on 07.09.2026.
//

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

struct LoopTargets
{
    llvm::BasicBlock* continueTarget;
    llvm::BasicBlock* breakTarget;
};

struct FieldInfo  { std::string name, type; bool is_public; std::string owner; };
struct MethodInfo { llvm::Function* fn; std::string ret; std::vector<std::string> args;
    bool is_public; int slot = -1; };

struct ClassInfo
{
    llvm::StructType* st = nullptr;
    std::string parent;
    std::vector<FieldInfo> fields;
    std::unordered_map<std::string, MethodInfo> methods;
    llvm::Function* ctor = nullptr;
    std::vector<std::string> ctor_args;
    llvm::Function* dtor = nullptr;
    std::vector<llvm::Function*> vslots;
    llvm::GlobalVariable* vtable = nullptr;

    int field_index(const std::string& n) const
    {
        for (size_t i = 0; i < fields.size(); ++i)
            if (fields[i].name == n) return (int)i;
        return -1;
    }
};

struct CodegenContext
{
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    std::unordered_map<std::string, llvm::AllocaInst*> variables;
    std::unordered_map<std::string, llvm::Function*> functions;
    std::vector<LoopTargets> loop_stack;
    std::unordered_set<std::string> const_vars;
    std::unordered_map<std::string, llvm::Function*> methods;

    std::unordered_map<std::string, std::string> var_types;
    std::unordered_map<llvm::Value*, std::string> value_types;

    std::unordered_map<std::string, ClassInfo> classes;
    std::unordered_map<std::string, std::string> func_ret;
    std::string current_class;
    std::string current_ret;
};

