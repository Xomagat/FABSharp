//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>

#include "AssigementStatement.h"

#include "Expression.h"

class BinExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr1, expr2;
    char op;

public:
    explicit BinExpression(char op, std::unique_ptr<Expression> expr1, std::unique_ptr<Expression> expr2) : op(op), expr1(std::move(expr1)), expr2(std::move(expr2))
    {

    }

    llvm::Value *codegen(CodegenContext &context) const override
    {
        llvm::Value* left = expr1->codegen(context);
        llvm::Value* right = expr2->codegen(context);

        bool isFloat = left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy();
        llvm::Type* fp_type = (left->getType()->isDoubleTy() || right->getType()->isDoubleTy())
                    ? left->getType()->getContext(), context.builder.getDoubleTy()
                    : context.builder.getFloatTy();

        if (isFloat)
        {
            left  = coerceToType(left,  fp_type, context.builder);
            right = coerceToType(right, fp_type, context.builder);

            switch (op)
            {
            case '+': return context.builder.CreateFAdd(left, right);
            case '-': return context.builder.CreateFSub(left, right);
            case '*': return context.builder.CreateFMul(left, right);
            case '/': return context.builder.CreateFDiv(left, right);
            case '%': return context.builder.CreateFRem(left, right);
            default: throw std::runtime_error("Codegen for this operator not implemented yet!");
            }
        }

        if (!left->getType()->isIntegerTy() || !right->getType()->isIntegerTy())
            throw std::runtime_error("Codegen only supports integer/float arithmetic for now!");

        switch (op)
        {
        case '+': return context.builder.CreateAdd(left, right);
        case '-': return context.builder.CreateSub(left, right);
        case '*': return context.builder.CreateMul(left, right);
        case '/': return context.builder.CreateSDiv(left, right);
        case '%': return context.builder.CreateSRem(left, right);
        default: throw std::runtime_error("Codegen for this operator not implemented yet!");
        }
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};