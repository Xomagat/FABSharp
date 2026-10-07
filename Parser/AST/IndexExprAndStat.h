//
// Created by Xomagat on 07.10.2026.
//

#pragma once
#include <memory>
#include <string>

#include "AssigementStatement.h"
#include "Expression.h"
#include "Statement.h"

class IndexExpression : public Expression
{
private:
    std::unique_ptr<Expression> target, index;

public:
    IndexExpression(std::unique_ptr<Expression> target, std::unique_ptr<Expression> index)
        : target(std::move(target)), index(std::move(index)) {}

    llvm::Value* codegen_ptr(CodegenContext& ctx, std::string& elem_out) const
    {
        auto& b = ctx.builder;

        llvm::Value* list = target->codegen(ctx);
        std::string t = type_of(ctx, list);
        if (!t.starts_with("list<"))
            throw std::runtime_error("Cannot assign by index to type '" + t + "'!");

        elem_out = list_elem(t);
        llvm::Type* et = type_to_llvm(elem_out, b);

        llvm::Value* idx = coerceToType(index->codegen(ctx), b.getInt64Ty(), b);
        llvm::Value* data = b.CreateLoad(b.getPtrTy(), list, "list.data");
        return b.CreateGEP(et, data, idx, "list.elem");
    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;

        llvm::Value* base = target->codegen(ctx);
        std::string t = type_of(ctx, base);

        llvm::Value* idx = coerceToType(index->codegen(ctx), b.getInt64Ty(), b);

        if (t == "string")
        {
            llvm::Value* p = b.CreateGEP(b.getInt8Ty(), base, idx);
            return b.CreateLoad(b.getInt8Ty(), p);
        }

        if (!t.starts_with("list<"))
            throw std::runtime_error("Type '" + t + "' does not support indexing!");

        std::string elem = list_elem(t);
        llvm::Type* et = type_to_llvm(elem, b);

        llvm::Value* data = b.CreateLoad(b.getPtrTy(), base, "list.data");
        llvm::Value* p = b.CreateGEP(et, data, idx, "list.elem");
        llvm::Value* v = b.CreateLoad(et, p);
        ctx.value_types[v] = elem;
        return v;
    }

    std::string to_str() const override { return target->to_str() + "[...]"; }
};

class IndexAssignStatement : public Statement
{
private:
    std::unique_ptr<Expression> target, value;

public:
    IndexAssignStatement(std::unique_ptr<Expression> target, std::unique_ptr<Expression> value)
        : target(std::move(target)), value(std::move(value)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto* ie = dynamic_cast<const IndexExpression*>(target.get());
        if (!ie)
            throw std::runtime_error("Left side of assignment must be a variable or a list element!");

        std::string elem;
        llvm::Value* ptr = ie->codegen_ptr(ctx, elem);

        llvm::Value* v = value->codegen_expected(ctx, elem);
        v = coerceToType(v, type_to_llvm(elem, ctx.builder), ctx.builder);
        ctx.builder.CreateStore(v, ptr);
    }
};
