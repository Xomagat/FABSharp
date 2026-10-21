//
// Created by Xomagat on 21.10.2026.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "llvm/IR/InlineAsm.h"

#include "AssigementStatement.h"
#include "Expression.h"
#include "Statement.h"

inline llvm::Value* emit_size_of(CodegenContext& ctx, llvm::Type* t)
{
    auto& b = ctx.builder;
    auto* nullp = llvm::ConstantPointerNull::get(b.getPtrTy());
    return b.CreatePtrToInt(b.CreateGEP(t, nullp, {b.getInt32(1)}), b.getInt64Ty());
}

class SizeOfExpression : public Expression
{
    std::string type_name;
    std::unique_ptr<Expression> expr;

public:
    explicit SizeOfExpression(std::string t) : type_name(std::move(t)) {}
    explicit SizeOfExpression(std::unique_ptr<Expression> e) : expr(std::move(e)) {}

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        std::string t = type_name;
        if (expr) t = type_of(ctx, expr->codegen(ctx));

        if (t == "void")
            throw std::runtime_error("size_of(void) is not allowed!");

        return emit_size_of(ctx, type_to_llvm(t, ctx.builder));
    }

    std::string to_str() const override { return "size_of(...)"; }
};

class TypeOfExpression : public Expression
{
    std::unique_ptr<Expression> expr;

public:
    explicit TypeOfExpression(std::unique_ptr<Expression> e) : expr(std::move(e)) {}

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* v = expr->codegen(ctx);
        return ctx.builder.CreateGlobalStringPtr(type_of(ctx, v));
    }

    std::string to_str() const override { return "type(...)"; }
};

class AsmStatement : public Statement
{
    std::string code, constraints;
    std::vector<std::unique_ptr<Expression>> args;

public:
    AsmStatement(std::string c, std::string k) : code(std::move(c)), constraints(std::move(k)) {}
    void add_arg(std::unique_ptr<Expression> a) { args.push_back(std::move(a)); }

    void codegen(CodegenContext& ctx) const override
    {
        std::vector<llvm::Value*> vals;
        std::vector<llvm::Type*> types;
        for (auto& a : args)
        {
            vals.push_back(a->codegen(ctx));
            types.push_back(vals.back()->getType());
        }

        auto* fty = llvm::FunctionType::get(ctx.builder.getVoidTy(), types, false);
        auto* ia  = llvm::InlineAsm::get(fty, code, constraints,
                                         true,
                                         false,
                                         llvm::InlineAsm::AD_ATT);
        ctx.builder.CreateCall(fty, ia, vals);
    }
};
