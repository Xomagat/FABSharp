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

        llvm::Type* fp_type = fp_result_type(left, right, context.builder);

        if (left->getType()->isPointerTy() || right->getType()->isPointerTy())
        {
            if (op != '+')
                throw std::runtime_error("Only '+' is supported for strings!");

            auto& b = context.builder;
            llvm::Type* ptr = b.getInt8Ty()->getPointerTo();
            llvm::Type* i64 = b.getInt64Ty();

            auto strlen = context.module.getOrInsertFunction(
                "strlen", llvm::FunctionType::get(i64, {ptr}, false));
            auto malloc = context.module.getOrInsertFunction(
                "malloc", llvm::FunctionType::get(ptr, {i64}, false));
            auto memcpy = context.module.getOrInsertFunction(
                "memcpy", llvm::FunctionType::get(ptr, {ptr, ptr, i64}, false));

            auto la = b.CreateCall(strlen, {left});
            auto lb = b.CreateCall(strlen, {right});
            auto total = b.CreateAdd(b.CreateAdd(la, lb), b.getInt64(1));
            auto buf = b.CreateCall(malloc, {total});

            b.CreateCall(memcpy, {buf, left, la});
            auto tail = b.CreateGEP(b.getInt8Ty(), buf, la);
            b.CreateCall(memcpy, {tail, right, b.CreateAdd(lb, b.getInt64(1))});
            return buf;
        }

        if (left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy())
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

        unsigned lw = left->getType()->getIntegerBitWidth();
        unsigned rw = right->getType()->getIntegerBitWidth();
        llvm::Type* wide = (lw >= rw) ? left->getType() : right->getType();
        left  = coerceToType(left,  wide, context.builder);
        right = coerceToType(right, wide, context.builder);

        switch (op)
        {
        case '+': return context.builder.CreateNSWAdd(left, right);
        case '-': return context.builder.CreateNSWSub(left, right);
        case '*': return context.builder.CreateNSWMul(left, right);
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