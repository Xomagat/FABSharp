//
// Created by Xomagat on 07.09.2026.
//

#pragma once
#include <string>
#include <unordered_map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

struct LoopTargets
{
    llvm::BasicBlock* continueTarget;
    llvm::BasicBlock* breakTarget;
};

struct CodegenContext
{
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    std::unordered_map<std::string, llvm::AllocaInst*> variables;
    std::unordered_map<std::string, llvm::Function*> functions;
    std::vector<LoopTargets> loop_stack;
};