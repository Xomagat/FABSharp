//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <iostream>

#include "Expression.h"
#include "Statement.h"

#include "../../libs/Environment.h"
#include "../../CodeGen/CodegenContext.h"

class WritelnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit WritelnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment& env) const override
    {
        std::cout << expr->eval(env)->as_string() << std::endl;
    }

    void codegen(CodegenContext &context) const override
    {
        auto printfType = llvm::FunctionType::get(
            context.builder.getInt32Ty(), {context.builder.getInt8Ty()->getPointerTo()}, true);
        auto printfFunc = context.module.getOrInsertFunction("printf", printfType);

        llvm::Value* val = expr->codegen(context);

        if (val->getType()->isIntegerTy())
        {
            llvm::Value* fmt = context.builder.CreateGlobalStringPtr("%d\n");
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

    void execute(Environment& env) const override
    {
        std::cout << expr->eval(env)->as_string();
    }

    void codegen(CodegenContext& context) const override
    {
        auto printfType = llvm::FunctionType::get(
            context.builder.getInt32Ty(), {context.builder.getInt8Ty()->getPointerTo()}, true);
        auto printfFunc = context.module.getOrInsertFunction("printf", printfType);

        llvm::Value* val = expr->codegen(context);

        if (val->getType()->isIntegerTy())
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

    void execute(Environment& env) const override
    {
        Val* target = env.revolve(name);
        if (!target)
            throw std::runtime_error("Variable {" + name + "} not found!");

        std::string input;
        std::getline(std::cin, input);

        auto raw = std::make_unique<StringValue>(input);

        auto it = coercers.find(target->type);
        std::unique_ptr<Value> converted =
            (it != coercers.end()) ? it->second(raw.get()) : nullptr;

        if (!converted)
            throw std::runtime_error("Cannot convert input to type " + target->type);

        env.assign(name, std::move(converted));
    }

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

        if (varType->isIntegerTy(32))
        {
            llvm::Value* fmt = context.builder.CreateGlobalStringPtr("%d");
            context.builder.CreateCall(scanfFunc, {fmt, alloc});
        }
        else
        {
            throw std::runtime_error("input_in codegen only supports int for now!");
        }
    }
};