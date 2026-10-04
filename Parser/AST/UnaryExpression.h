//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>

#include "Expression.h"

class UnaryExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr;
    char op;

public:
    explicit UnaryExpression(char op, std::unique_ptr<Expression> expr) : op(op), expr(std::move(expr)) {}

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* val = expr->codegen(ctx);

        switch (op)
        {
        case '-':
            if (val->getType()->isDoubleTy())
                return ctx.builder.CreateFNeg(val);
            if (val->getType()->isIntegerTy())
                return ctx.builder.CreateNeg(val);
            throw std::runtime_error("Codegen unary '-' not supported for this type!");

        case '+':
            if (val->getType()->isDoubleTy() || val->getType()->isIntegerTy())
                return val;
            throw std::runtime_error("Codegen unary '+' not supported for this type!");

        default:
            throw std::runtime_error("Codegen for this unary operator not implemented yet!");
        }
    }

    std::string to_str() const override
    {
        return op + expr->to_str();
    }
};