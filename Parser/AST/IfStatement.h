//
// Created by Xomagat on 10.08.2026.
//

#pragma once
#include <memory>

#include "Expression.h"
#include "Statement.h"

class IfStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> if_statement, else_statement;

public:
    explicit IfStatement(std::unique_ptr<Expression> condition,
        std::unique_ptr<Statement> if_statement,
        std::unique_ptr<Statement> else_statement) : condition(std::move(condition)),
                                                     if_statement(std::move(if_statement)),
                                                     else_statement(std::move(else_statement))
    {

    }

    void codegen(CodegenContext &context) const override
    {
        auto condVal = condition->codegen(context);

        if (!condVal->getType()->isIntegerTy(1))
        {
            condVal = context.builder.CreateICmpNE(
                condVal, llvm::ConstantInt::get(condVal->getType(), 0), "ifcond");
        }

        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context.context, "then", function);
        llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context.context, "else", function);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context.context, "ifcont", function);

        context.builder.CreateCondBr(condVal, thenBB, elseBB);

        context.builder.SetInsertPoint(thenBB);
        if_statement->codegen(context);
        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(mergeBB);

        context.builder.SetInsertPoint(elseBB);
        if (else_statement)
            else_statement->codegen(context);
        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(mergeBB);

        context.builder.SetInsertPoint(mergeBB);
    }
};

class BlockStatement : public Statement
{
private:
    std::vector<std::unique_ptr<Statement>> statements;

public:
    explicit BlockStatement(std::vector<std::unique_ptr<Statement>> statements) : statements(std::move(statements)) {}

    void codegen(CodegenContext &context) const override
    {
        auto saveVar = context.variables;
        auto saveConst = context.const_vars;

        for (auto& s : statements)
            s->codegen(context);

        context.variables = saveVar;
        context.const_vars = saveConst;
    }
};