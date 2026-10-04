//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>

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

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};