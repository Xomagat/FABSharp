//
// Created by Xomagat on 07.09.2026.
//

#ifndef FABSHARP_COMPILE_H
#define FABSHARP_COMPILE_H

#include <memory>
#include <vector>
#include <string>
#include <iostream>

#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Passes/PassBuilder.h"

#include "../Parser/AST/FunctionStatement.h"
#include "../Parser/AST/ClassStatement.h"
#include "../Parser/AST/Statement.h"
#include "CodegenContext.h"

struct CompileOptions
{
    int  opt_level = 2;               // 0,1,2,3
    char size_opt  = 0;               // 's' или 'z' (перекрывает opt_level)
    std::string cpu = "x86-64-v3";
    std::string ir_path;              // не пусто -> записать .ll
    std::string asm_path;             // не пусто -> записать .s
    bool skip_obj = false;            // не писать объектный файл
    bool verbose  = false;
};

void compile(std::vector<std::unique_ptr<Statement>>& statements,
             const std::string& obj_path, const CompileOptions& opts);

#ifdef _WIN32
inline constexpr const char* OBJ_EXT = ".obj";
#else
inline constexpr const char* OBJ_EXT = ".o";
#endif

#endif // FABSHARP_COMPILE_H
