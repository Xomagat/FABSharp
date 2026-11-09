//
// Created by Xomagat on 21.10.2026.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "llvm/IR/InlineAsm.h"

#include "Expression.h"
#include "FunctionalExpression.h"

#include "Statement.h"
#include "AssigementStatement.h"

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

class ToStrExpression : public Expression
{
    std::unique_ptr<Expression> expr;

public:
    explicit ToStrExpression(std::unique_ptr<Expression> e) : expr(std::move(e)) {}

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* v = expr->codegen(ctx);
        std::string t = type_of(ctx, v);

        if (t == "string") return v;

        const StdlibFunctionInfo* info = find_lib_method(t, "to_str", {});
        if (!info)
            throw std::runtime_error("Cannot put a value of type '" + t + "' into an f-string!");

        llvm::Value* r = FunctionalExpression::emit_stdlib_call(ctx, *info, {v});
        ctx.value_types[r] = "string";
        return r;
    }

    std::string to_str() const override { return "to_str(" + expr->to_str() + ")"; }
};

class ConcatExpression : public Expression
{
    std::vector<std::unique_ptr<Expression>> parts;

public:
    void add(std::unique_ptr<Expression> e) { parts.push_back(std::move(e)); }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        auto* arrTy = llvm::ArrayType::get(b.getPtrTy(), parts.size());
        auto* arr   = entry_alloca(b, arrTy, "concat.parts");

        for (size_t i = 0; i < parts.size(); ++i)
            b.CreateStore(parts[i]->codegen(ctx), b.CreateConstGEP2_32(arrTy, arr, 0, (unsigned)i));

        auto fn = ctx.module.getOrInsertFunction("fab_concat_n",
            llvm::FunctionType::get(b.getPtrTy(), {b.getPtrTy(), b.getInt32Ty()}, false));
        llvm::Value* r = b.CreateCall(fn, {arr, b.getInt32((unsigned)parts.size())});
        ctx.value_types[r] = "string";
        return r;
    }

    std::string to_str() const override { return "concat(...)"; }
};