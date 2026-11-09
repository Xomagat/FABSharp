//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <iostream>

#include "Expression.h"

#include "AssigementStatement.h"
#include "Statement.h"

#include "../../CodeGen/CodegenContext.h"

inline void emit_print(CodegenContext& ctx, llvm::Value* val, bool newline)
{
    auto& b = ctx.builder;

    std::string t = type_of(ctx, val);
    if (is_class(t) || is_lib_type(t))
        throw std::runtime_error("Cannot print an object of class '" + t + "': classes are not built-in types!");

    auto nl = [&]()
    {
        auto fn = ctx.module.getOrInsertFunction("fab_out_nl",
            llvm::FunctionType::get(b.getVoidTy(), {}, false));
        b.CreateCall(fn);
    };

    auto rt = [&](const char* kind, llvm::Value* arg)
    {
        std::string name = std::string("fab_out_") + kind + (newline ? "ln" : "");
        auto fn = ctx.module.getOrInsertFunction(name,
            llvm::FunctionType::get(b.getVoidTy(), {arg->getType()}, false));
        b.CreateCall(fn, {arg});
    };

    // list<T> and dict<K,V> have their own printers
    if (emit_container_print(ctx, val, t))
    {
        if (newline) nl();
        return;
    }

    llvm::Type* ty = val->getType();

    if (ty->isIntegerTy(1))
    {
        rt("str", b.CreateSelect(val, b.CreateGlobalStringPtr("true"), b.CreateGlobalStringPtr("false")));
    }
    else if (ty->isX86_FP80Ty())
    {
        auto printfType = llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, true);
        auto printfFunc = ctx.module.getOrInsertFunction("printf", printfType);
        b.CreateCall(printfFunc, {b.CreateGlobalStringPtr(newline ? "%.21Lg\n" : "%.21Lg"), val});
    }
    else if (ty->isFloatTy())   rt("f32", val);
    else if (ty->isDoubleTy())  rt("f64", val);
    else if (ty->isIntegerTy(8)) rt("char", val);
    else if (ty->isIntegerTy(16) || ty->isIntegerTy(32)) rt("i32", b.CreateSExt(val, b.getInt32Ty()));
    else if (ty->isIntegerTy(64)) rt("i64", val);
    else if (ty->isPointerTy())  rt("str", val);
    else
        throw std::runtime_error("Cannot print a value of type '" + t + "'!");
}

class WritelnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WritelnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void codegen(CodegenContext &context) const override
    {
        emit_print(context, expr->codegen(context), true);
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
        emit_print(context, expr->codegen(context), false);
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
        else if (varType->isPointerTy()) // string
        {
            auto& b = context.builder;
            auto allocFn = context.module.getOrInsertFunction("fab_alloc",
                llvm::FunctionType::get(b.getPtrTy(), {b.getInt64Ty()}, false));
            llvm::Value* stringBuffer = b.CreateCall(allocFn, {b.getInt64(256)});

            b.CreateStore(b.getInt8(0), stringBuffer);
            b.CreateStore(stringBuffer, alloc);

            fmt = b.CreateGlobalStringPtr("%255s");
            arg = stringBuffer;
        }
        else if (varType->isIntegerTy(1)) // bool
        {
            llvm::AllocaInst* tmpAlloc = entry_alloca(context.builder, context.builder.getInt32Ty(), "tmp_bool");
            fmt = context.builder.CreateGlobalStringPtr("%d");

            auto& b = context.builder;
            llvm::Value* rc = b.CreateCall(scanfFunc, {fmt, arg});
            auto done = context.module.getOrInsertFunction("fab_input_done",
                llvm::FunctionType::get(b.getVoidTy(), {b.getInt32Ty()}, false));
            b.CreateCall(done, {rc});

            llvm::Value* tmpVal = context.builder.CreateLoad(context.builder.getInt32Ty(), tmpAlloc);
            llvm::Value* boolVal = context.builder.CreateICmpNE(tmpVal, context.builder.getInt32(0));
            context.builder.CreateStore(boolVal, alloc);
            return;
        }
        else
        {
            throw std::runtime_error("Unsupported type for input_in codegen!");
        }

        auto& b = context.builder;
        llvm::Value* rc = b.CreateCall(scanfFunc, {fmt, arg});
        auto done = context.module.getOrInsertFunction("fab_input_done",
            llvm::FunctionType::get(b.getVoidTy(), {b.getInt32Ty()}, false));
        b.CreateCall(done, {rc});
    }
};