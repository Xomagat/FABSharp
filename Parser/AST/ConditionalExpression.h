//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>

#include "AssigementStatement.h"

#include "Expression.h"

class ConditionalExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr1, expr2;
    std::string op;

public:
    explicit ConditionalExpression(std::string op,
        std::unique_ptr<Expression> expr1,
        std::unique_ptr<Expression> expr2) : op(op), expr1(std::move(expr1)), expr2(std::move(expr2))
    {

    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        if (op == "&&" || op == "||")
            return codegen_logical(ctx);

        llvm::Value* left = expr1->codegen(ctx);
        llvm::Value* right = expr2->codegen(ctx);

        bool isFloat = left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy();
        llvm::Type* fp_type = (left->getType()->isDoubleTy() || right->getType()->isDoubleTy())
                    ? left->getType()->getContext(), ctx.builder.getDoubleTy()
                    : ctx.builder.getFloatTy();

        if (isFloat)
        {
            left  = coerceToType(left,  fp_type, ctx.builder);
            right = coerceToType(right, fp_type, ctx.builder);

            if (op == "==") return ctx.builder.CreateFCmpOEQ(left, right);
            if (op == "!=") return ctx.builder.CreateFCmpONE(left, right);
            if (op == "<")  return ctx.builder.CreateFCmpOLT(left, right);
            if (op == ">")  return ctx.builder.CreateFCmpOGT(left, right);
            if (op == "<=") return ctx.builder.CreateFCmpOLE(left, right);
            if (op == ">=") return ctx.builder.CreateFCmpOGE(left, right);
            throw std::runtime_error("Codegen for this conditional operator not implemented yet!");
        }

        if (op == "==") return ctx.builder.CreateICmpEQ(left, right);
        if (op == "!=") return ctx.builder.CreateICmpNE(left, right);
        if (op == "<")  return ctx.builder.CreateICmpSLT(left, right);
        if (op == ">")  return ctx.builder.CreateICmpSGT(left, right);
        if (op == "<=") return ctx.builder.CreateICmpSLE(left, right);
        if (op == ">=") return ctx.builder.CreateICmpSGE(left, right);
        throw std::runtime_error("Codegen for this conditional operator not implemented yet!");
    }

    llvm::Value* codegen_logical(CodegenContext& ctx) const
    {
        auto& b = ctx.builder;

        llvm::Function* func = b.GetInsertBlock()->getParent();

        llvm::Value* expr = to_bool(expr1->codegen(ctx), b);
        llvm::BasicBlock* block = b.GetInsertBlock();

        auto rhsBB = llvm::BasicBlock::Create(ctx.context, "logic.rhs", func);
        auto endBB = llvm::BasicBlock::Create(ctx.context, "logic.end", func);

        if (op == "&&") b.CreateCondBr(expr, rhsBB, endBB);
        else            b.CreateCondBr(expr, endBB, rhsBB);

        b.SetInsertPoint(rhsBB);
        llvm::Value* rhs = to_bool(expr2->codegen(ctx), b);
        llvm::BasicBlock* block2 = b.GetInsertBlock();
        b.CreateBr(endBB);

        b.SetInsertPoint(endBB);
        auto* phi = b.CreatePHI(b.getInt1Ty(), 2, "logic.res");
        phi->addIncoming(b.getInt1(op == "||"), block);
        phi->addIncoming(rhs, block2);
        return phi;
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};