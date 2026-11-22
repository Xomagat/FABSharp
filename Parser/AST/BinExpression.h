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
        auto& b = context.builder;

        llvm::Value* left = expr1->codegen(context);
        llvm::Value* right = expr2->codegen(context);

        llvm::Type* fp_type = fp_result_type(left, right, b);

        if (left->getType()->isPointerTy() || right->getType()->isPointerTy())
        {
            if (op != '+')
                throw std::runtime_error("Only '+' is supported for strings!");

            if (type_of(context, left) != "string" || type_of(context, right) != "string")
                throw std::runtime_error("'+' needs a string on both sides: use $\"...{x}...\" or x.to_str()");

            auto fn = context.module.getOrInsertFunction("fab_concat2",
                llvm::FunctionType::get(b.getPtrTy(), {b.getPtrTy(), b.getPtrTy()}, false));
            if (auto* f = llvm::dyn_cast<llvm::Function>(fn.getCallee())) f->setDoesNotThrow();

            llvm::Value* r = b.CreateCall(fn, {left, right});
            context.value_types[r] = "string";
            return r;
        }

        if (left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy())
        {
            left  = coerceToType(left,  fp_type, b);
            right = coerceToType(right, fp_type, b);

            switch (op)
            {
            case '+': return b.CreateFAdd(left, right);
            case '-': return b.CreateFSub(left, right);
            case '*': return b.CreateFMul(left, right);
            case '/': return b.CreateFDiv(left, right);
            case '%': return b.CreateFRem(left, right);
            default: throw std::runtime_error("Codegen for this operator not implemented yet!");
            }
        }

        if (!left->getType()->isIntegerTy() || !right->getType()->isIntegerTy())
            throw std::runtime_error("Codegen only supports integer/float arithmetic for now!");

        std::string lt = type_of(context, left), rt = type_of(context, right);
        unsigned lw = left->getType()->getIntegerBitWidth();
        unsigned rw = right->getType()->getIntegerBitWidth();

        unsigned w = std::max({lw, rw, 32u});
        bool un = (is_unsigned(lt) && lw == w) || (is_unsigned(rt) && rw == w);
        std::string res = un ? (w == 64 ? "unlong" : "unint")
                             : (w == 64 ? "long"   : "int");

        llvm::Type* wide = llvm::Type::getIntNTy(context.context, w);
        left  = coerceToType(left,  wide, b);
        right = coerceToType(right, wide, b);

        switch (op)
        {
        case '+': return tag(context, b.CreateAdd(left, right), res);
        case '-': return tag(context, b.CreateSub(left, right), res);
        case '*': return tag(context, b.CreateMul(left, right), res);
        case '/':
        case '%':
        {
            emit_fail_if(context, b.CreateICmpEQ(right, llvm::ConstantInt::get(wide, 0)),
                         "Division by zero");

            if (un)
                return tag(context, op == '/' ? b.CreateUDiv(left, right)
                                              : b.CreateURem(left, right), res);

            llvm::Value* isM1 = b.CreateICmpEQ(right, llvm::ConstantInt::getSigned(wide, -1));
            llvm::Value* safe = b.CreateSelect(isM1, llvm::ConstantInt::get(wide, 1), right);

            if (op == '/')
                return b.CreateSelect(isM1, b.CreateNeg(left), b.CreateSDiv(left, safe));
            return b.CreateSelect(isM1, llvm::ConstantInt::get(wide, 0), b.CreateSRem(left, safe));
        }
        default: throw std::runtime_error("Codegen for this operator not implemented yet!");
        }
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};