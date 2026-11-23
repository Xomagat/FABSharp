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

class TernaryExpression : public Expression
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Expression> true_expr;
    std::unique_ptr<Expression> false_expr;

public:
    explicit TernaryExpression(std::unique_ptr<Expression> condition,
                               std::unique_ptr<Expression> true_expr,
                               std::unique_ptr<Expression> false_expr)
        : condition(std::move(condition)),
          true_expr(std::move(true_expr)),
          false_expr(std::move(false_expr))
    {
    }

    llvm::Value* codegen(CodegenContext &context) const override
    {
        auto condVal = condition->codegen(context);

        if (!condVal->getType()->isIntegerTy(1))
        {
            condVal = context.builder.CreateICmpNE(
                condVal, llvm::ConstantInt::get(condVal->getType(), 0), "terncond");
        }

        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context.context, "tern.then", function);
        llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context.context, "tern.else", function);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context.context, "tern.cont", function);

        context.builder.CreateCondBr(condVal, thenBB, elseBB);

        context.builder.SetInsertPoint(thenBB);
        llvm::Value* thenVal = true_expr->codegen(context);
        thenBB = context.builder.GetInsertBlock();
        context.builder.CreateBr(mergeBB);

        context.builder.SetInsertPoint(elseBB);
        llvm::Value* elseVal = false_expr->codegen(context);
        elseBB = context.builder.GetInsertBlock();
        context.builder.CreateBr(mergeBB);

        context.builder.SetInsertPoint(mergeBB);

        if (thenVal->getType() != elseVal->getType())
        {
            throw std::runtime_error("Ternary branches must have the same type!");
        }

        llvm::PHINode* phi = context.builder.CreatePHI(thenVal->getType(), 2, "terntmp");
        phi->addIncoming(thenVal, thenBB);
        phi->addIncoming(elseVal, elseBB);

        return phi;
    }

    std::string to_str() const override
    {
        return condition->to_str() + " ? " + true_expr->to_str() + " : " + false_expr->to_str();
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
        auto save_var_types = context.var_types;

        for (auto& s : statements)
        {
            try { s->gen(context); }
            catch (const FabError& e) { g_diags.push_back(e); }
        }

        context.variables = saveVar;
        context.var_types = save_var_types;
        context.const_vars = saveConst;
    }
};