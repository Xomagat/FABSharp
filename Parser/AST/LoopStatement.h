//
// Created by Xomagat on 13.08.2026.
//

#pragma once
#include "BreakStatement.h"
#include "ContinueStatement.h"

#include "Expression.h"
#include "Statement.h"

class WhileStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> while_statement;

public:
    explicit WhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> while_statement) : condition(std::move(condition)), while_statement(std::move(while_statement)) {}

    void execute(Environment &env) const override
    {
        while (condition->eval(env)->as_bool())
        {
            try
            {
                while_statement->execute(env);
            }
            catch (const BreakStatement&)
            {
                break;
            }
            catch (const ContinueStatement&)
            {
                continue;
            }
        }
    }

    void codegen(CodegenContext &context) const override
    {
        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context.context, "loopcond", function);
        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context.context, "loopbody", function);
        llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(context.context, "loopend", function);

        context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(condBB);
        llvm::Value* condVal = condition->codegen(context);
        if (!condVal->getType()->isIntegerTy(1))
            condVal = context.builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "loopcondval");
        context.builder.CreateCondBr(condVal, bodyBB, endBB);

        context.builder.SetInsertPoint(bodyBB);
        context.loop_stack.push_back({condBB, endBB});
        while_statement->codegen(context);
        context.loop_stack.pop_back();

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(endBB);
    }
};

class DoWhileStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> while_statement;

public:
    explicit DoWhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> while_statement) : condition(std::move(condition)), while_statement(std::move(while_statement)) {}

    void execute(Environment &env) const override
    {
        do
        {
            try
            {
                while_statement->execute(env);
            }
            catch (const BreakStatement&)
            {
                break;
            }
            catch (const ContinueStatement&)
            {
                continue;
            }
        } while (condition->eval(env)->as_bool());
    }

    void codegen(CodegenContext& context) const override
    {
        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context.context, "loopbody", function);
        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context.context, "loopcond", function);
        llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(context.context, "loopend", function);

        context.builder.CreateBr(bodyBB);

        context.builder.SetInsertPoint(bodyBB);
        context.loop_stack.push_back({condBB, endBB});
        while_statement->codegen(context);
        context.loop_stack.pop_back();

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(condBB);
        llvm::Value* condVal = condition->codegen(context);
        if (!condVal->getType()->isIntegerTy(1))
            condVal = context.builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "dowhilecondval");
        context.builder.CreateCondBr(condVal, bodyBB, endBB);

        context.builder.SetInsertPoint(endBB);
    }
};

class ForStatement : public Statement
{
private:
    std::unique_ptr<Statement> initialization;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> increment;
    std::unique_ptr<Statement> for_statement;

public:
    explicit ForStatement(std::unique_ptr<Statement> initialization, std::unique_ptr<Expression> condition,
        std::unique_ptr<Statement> increment, std::unique_ptr<Statement> for_statement) : initialization(std::move(initialization)),
                                                                                          condition(std::move(condition)),
                                                                                          increment(std::move(increment)),
                                                                                          for_statement(std::move(for_statement)) {}

    void execute(Environment &env) const override
    {
        for (initialization->execute(env); condition->eval(env)->as_bool(); increment->execute(env))
        {
            try
            {
                for_statement->execute(env);
            }
            catch (const BreakStatement&)
            {
                break;
            }
            catch (const ContinueStatement&)
            {
                continue;
            }
        }
    }

    void codegen(CodegenContext& context) const override
    {
        initialization->codegen(context);

        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context.context, "loopcond", function);
        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context.context, "loopbody", function);
        llvm::BasicBlock* incrBB = llvm::BasicBlock::Create(context.context, "loopincr", function);
        llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(context.context, "loopend", function);

        context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(condBB);
        llvm::Value* condVal = condition->codegen(context);
        if (!condVal->getType()->isIntegerTy(1))
            condVal = context.builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "forcondval");
        context.builder.CreateCondBr(condVal, bodyBB, endBB);

        context.builder.SetInsertPoint(bodyBB);
        context.loop_stack.push_back({incrBB, endBB});   // continue -> incrBB, не condBB!
        for_statement->codegen(context);
        context.loop_stack.pop_back();

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(incrBB);

        context.builder.SetInsertPoint(incrBB);
        increment->codegen(context);
        context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(endBB);
    }
};