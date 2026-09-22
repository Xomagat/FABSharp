//
// Created by Xomagat on 07.09.2026.
//

#ifndef FABSHARP_COMPILE_H
#define FABSHARP_COMPILE_H

#include <memory>
#include <vector>
#include <string>

#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"

#include "../Parser/AST/FunctionStatement.h"
#include "../Parser/AST/Statement.h"
#include "CodegenContext.h"

void compile(std::vector<std::unique_ptr<Statement>>& statements, std::string name);

#endif // FABSHARP_COMPILE_H
