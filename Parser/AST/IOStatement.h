//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <iostream>

#include "Expression.h"
#include "Statement.h"

#include "../../CodeGen/CodegenContext.h"

class WritelnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WritelnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void codegen(CodegenContext &context) const override
    {
        auto printfType = llvm::FunctionType::get(
            context.builder.getInt32Ty(), {context.builder.getInt8Ty()->getPointerTo()}, true);
        auto printfFunc = context.module.getOrInsertFunction("printf", printfType);

        llvm::Value* val = expr->codegen(context);

        std::string t = type_of(context, val);
        if (t.starts_with("list<"))
        {
            auto printFn = context.module.getOrInsertFunction(
                "fab_list_print_" + list_elem(t),
                llvm::FunctionType::get(context.builder.getVoidTy(),
                                        {context.builder.getInt8Ty()->getPointerTo()}, false));
            context.builder.CreateCall(printFn, {val});
            context.builder.CreateCall(printfFunc, {context.builder.CreateGlobalStringPtr("\n")});
            return;
        }

        if (val->getType()->isIntegerTy(1))
            val = context.builder.CreateZExt(val, context.builder.getInt32Ty());

        if (val->getType()->isFloatingPointTy())
        {
            llvm::Value* fmt = nullptr;
            if (val->getType()->isFloatTy())
            {
                val = context.builder.CreateFPExt(val, context.builder.getFloatTy());
                fmt = context.builder.CreateGlobalStringPtr("%.9g\n");
            }
            else
            {
                val = context.builder.CreateFPExt(val, context.builder.getDoubleTy());
                fmt = context.builder.CreateGlobalStringPtr("%.17g\n");
            }
            context.builder.CreateCall(printfFunc, {fmt, val});
        }
        else if (val->getType()->isIntegerTy(8))
        {
            val = context.builder.CreateZExt(val, context.builder.getInt32Ty());
            llvm::Value* fmt = context.builder.CreateGlobalStringPtr("%c\n");
            context.builder.CreateCall(printfFunc, {fmt, val});
        }
        else if (val->getType()->isIntegerTy())
        {
            unsigned bits = val->getType()->getIntegerBitWidth();
            if (bits < 32)
                val = context.builder.CreateSExt(val, context.builder.getInt32Ty());

            const char* f = (bits == 64) ? "%lld\n" : "%d\n";
            llvm::Value* fmt = context.builder.CreateGlobalStringPtr(f);
            context.builder.CreateCall(printfFunc, {fmt, val});
        }
        else
        {
            context.builder.CreateCall(printfFunc, {val});
            context.builder.CreateCall(printfFunc, {context.builder.CreateGlobalStringPtr("\n")});
        }
    }
};

class WriteStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WriteStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void codegen(CodegenContext& context) const override
    {
        auto printfType = llvm::FunctionType::get(
            context.builder.getInt32Ty(), {context.builder.getInt8Ty()->getPointerTo()}, true);
        auto printfFunc = context.module.getOrInsertFunction("printf", printfType);

        llvm::Value* val = expr->codegen(context);

        std::string t = type_of(context, val);
        if (t.starts_with("list<"))
        {
            auto printFn = context.module.getOrInsertFunction(
                "fab_list_print_" + list_elem(t),
                llvm::FunctionType::get(context.builder.getVoidTy(),
                                        {context.builder.getInt8Ty()->getPointerTo()}, false));
            context.builder.CreateCall(printFn, {val});
            return;
        }

        if (val->getType()->isIntegerTy(1))
            val = context.builder.CreateZExt(val, context.builder.getInt32Ty());

        if (val->getType()->isFloatingPointTy())
        {
            llvm::Value* fmt = nullptr;
            if (val->getType()->isFloatTy())
            {
                val = context.builder.CreateFPExt(val, context.builder.getFloatTy());
                fmt = context.builder.CreateGlobalStringPtr("%.9g");
            }
            else
            {
                val = context.builder.CreateFPExt(val, context.builder.getDoubleTy());
                fmt = context.builder.CreateGlobalStringPtr("%.17g");
            }
            context.builder.CreateCall(printfFunc, {fmt, val});
        }
        else if (val->getType()->isIntegerTy(8))
        {
            unsigned bits = val->getType()->getIntegerBitWidth();
            if (bits < 32)
                val = context.builder.CreateSExt(val, context.builder.getInt32Ty());

            const char* f = (bits == 64) ? "%lld" : "%d";
            llvm::Value* fmt = context.builder.CreateGlobalStringPtr(f);
            context.builder.CreateCall(printfFunc, {fmt, val});
        }
        else if (val->getType()->isIntegerTy())
        {
            llvm::Value* fmt = context.builder.CreateGlobalStringPtr("%d");
            context.builder.CreateCall(printfFunc, {fmt, val});
        }
        else
        {
            context.builder.CreateCall(printfFunc, {val});
        }
    }
};

class InputInStatement : public Statement
{
private:
    std::string name;

public:
    explicit InputInStatement(std::string name) : name(name) {}

    void codegen(CodegenContext& context) const override
    {
        auto it = context.variables.find(name);
        if (it == context.variables.end())
            throw std::runtime_error("Variable {" + name + "} not found (codegen)!");

        llvm::AllocaInst* alloc = it->second;
        llvm::Type* varType = alloc->getAllocatedType();

        auto scanfType = llvm::FunctionType::get(
            context.builder.getInt32Ty(), {context.builder.getInt8Ty()->getPointerTo()}, true);
        auto scanfFunc = context.module.getOrInsertFunction("scanf", scanfType);

        llvm::Value* fmt = nullptr;
        llvm::Value* arg = alloc;

        if (varType->isIntegerTy(32)) // int
        {
            fmt = context.builder.CreateGlobalStringPtr("%d");
        }
        else if (varType->isIntegerTy(8)) // char
        {
            fmt = context.builder.CreateGlobalStringPtr(" %c");
        }
        else if (varType->isIntegerTy(16)) // short
        {
            fmt = context.builder.CreateGlobalStringPtr("%hd");
        }
        else if (varType->isIntegerTy(64)) // long
        {
            fmt = context.builder.CreateGlobalStringPtr("%lld");
        }
        else if (varType->isFloatTy()) // float
        {
            fmt = context.builder.CreateGlobalStringPtr("%f");
        }
        else if (varType->isDoubleTy()) // double
        {
            fmt = context.builder.CreateGlobalStringPtr("%lf");
        }
        else if (varType->isPointerTy()) // string (i8*)
        {
            llvm::Value* arraySize = context.builder.getInt32(256);
            llvm::Value* stringBuffer = context.builder.CreateAlloca(context.builder.getInt8Ty(), arraySize, name + "_buf");

            context.builder.CreateStore(stringBuffer, alloc);

            fmt = context.builder.CreateGlobalStringPtr("%255s");
            arg = stringBuffer;
        }
        else if (varType->isIntegerTy(1)) // bool
        {
            llvm::AllocaInst* tmpAlloc = context.builder.CreateAlloca(context.builder.getInt32Ty(), nullptr, "tmp_bool");
            fmt = context.builder.CreateGlobalStringPtr("%d");

            context.builder.CreateCall(scanfFunc, {fmt, tmpAlloc});

            llvm::Value* tmpVal = context.builder.CreateLoad(context.builder.getInt32Ty(), tmpAlloc);
            llvm::Value* boolVal = context.builder.CreateICmpNE(tmpVal, context.builder.getInt32(0));
            context.builder.CreateStore(boolVal, alloc);
            return;
        }
        else
        {
            throw std::runtime_error("Unsupported type for input_in codegen!");
        }

        context.builder.CreateCall(scanfFunc, {fmt, arg});
    }
};