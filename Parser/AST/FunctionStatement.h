//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "FunctionalExpression.h"
#include "Statement.h"

class FunctionStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit FunctionStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void execute(Environment &env) const override
    {
        expr->eval(env);
    }
};

class FunctionDefineStatement : public Statement
{
private:
    std::string name;
    std::vector<std::string> arg_names;
    std::vector<std::string> arg_types;
    std::shared_ptr<Statement> body;

public:
    explicit FunctionDefineStatement(std::string name, std::vector<std::string> arg_types, std::vector<std::string> arg_names, std::unique_ptr<Statement> body)
        : name(name), arg_types(arg_types), arg_names(arg_names), body(std::move(body)) {}

    void execute(Environment &env) const override
    {
        Functions::define(name, std::make_unique<UserDefineFunction>(arg_types, arg_names, body));
    }

    void codegen(CodegenContext &context) const override
    {
        std::vector<llvm::Type*> param_types;
        for (auto& arg_type : arg_types)
            param_types.push_back(type_to_llvm(arg_type, context.builder));

        llvm::FunctionType* fn_type = llvm::FunctionType::get(
            context.builder.getInt32Ty(), param_types, false);

        llvm::Function* function = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, name, context.module);

        context.functions[name] = function;

        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context.context, "entry", function);
        auto save_insert_block = context.builder.GetInsertBlock();
        context.builder.SetInsertPoint(entry);

        auto save_variables = context.variables;

        int i = 0;
        for (auto& arg : function->args())
        {
            arg.setName(arg_names[i]);
            llvm::AllocaInst* alloc = context.builder.CreateAlloca(param_types[i], nullptr, arg_names[i]);
            context.builder.CreateStore(&arg, alloc);
            context.variables[arg_names[i]] = alloc;
            i++;
        }

        body->codegen(context);

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateRet(context.builder.getInt32(0));

        context.variables = save_variables;
        context.builder.SetInsertPoint(save_insert_block);
    }

    std::string get_name() const
    {
        return name;
    }
};