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

    void codegen(CodegenContext &context) const override
    {
        expr->codegen(context);
    }
};

class FunctionDefineStatement : public Statement
{
private:
    std::string type;
    std::string name;
    std::vector<std::string> arg_names;
    std::vector<std::string> arg_types;
    std::shared_ptr<Statement> body;

public:
    explicit FunctionDefineStatement(std::string& type, std::string& name, std::vector<std::string>& arg_types, std::vector<std::string>& arg_names, std::unique_ptr<Statement> body)
        : type(type), name(name), arg_types(arg_types), arg_names(arg_names), body(std::move(body)) {}

    void codegen(CodegenContext &context) const override
    {
        std::vector<llvm::Type*> param_types;
        for (auto& arg_type : arg_types)
            param_types.push_back(type_to_llvm(arg_type, context.builder));

        llvm::FunctionType* fn_type = llvm::FunctionType::get(
            type_to_llvm(type, context.builder), param_types, false);

        llvm::Function* function = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, name, context.module);

        context.functions[mangle_name(name, arg_types)] = function;

        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context.context, "entry", function);
        auto save_insert_block = context.builder.GetInsertBlock();
        context.builder.SetInsertPoint(entry);

        auto save_variables = context.variables;
        auto save_var_types = context.var_types;

        int i = 0;
        for (auto& arg : function->args())
        {
            arg.setName(arg_names[i]);
            llvm::AllocaInst* alloc = context.builder.CreateAlloca(param_types[i], nullptr, arg_names[i]);
            context.builder.CreateStore(&arg, alloc);
            context.variables[arg_names[i]] = alloc;
            context.var_types[arg_names[i]] = arg_types[i];
            i++;
        }

        body->codegen(context);

        if (!context.builder.GetInsertBlock()->getTerminator())
        {
            if (type == "void")
                context.builder.CreateRetVoid();
            else
                context.builder.CreateRet(llvm::Constant::getNullValue(type_to_llvm(type, context.builder)));
        }

        context.variables = save_variables;
        context.var_types = save_var_types;
        context.builder.SetInsertPoint(save_insert_block);
    }

    std::string get_name() const
    {
        return name;
    }
};