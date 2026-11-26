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

    void assign(CodegenContext& ctx, const Expression& value) const
    {
        auto& b = ctx.builder;
        llvm::Value* base = target->codegen(ctx);
        std::string t = type_of(ctx, base);

        if (t.starts_with("dict<"))
        {
            auto [k, v] = dict_kv(t);
            emit_null_check(ctx, base, "Null reference: dict index");
            llvm::Value* kv = index->codegen_expected(ctx, k);
            llvm::Value* vv = value.is_null_literal()
                ? llvm::Constant::getNullValue(type_to_llvm(v, b))
                : value.codegen_expected(ctx, v);
            emit_dict_set(ctx, t, base, kv, vv);
            return;
        }

        if (!t.starts_with("list<"))
            throw std::runtime_error("Cannot assign by index to type '" + t + "'!");

        std::string elem = list_elem(t);
        llvm::Type* et = type_to_llvm(elem, b);
        llvm::Value* idx = coerceToType(index->codegen(ctx), b.getInt64Ty(), b);

        emit_null_check(ctx, base, "Null reference: list index");
        emit_list_bounds(ctx, base, idx);

        llvm::Value* v = value.is_null_literal()
            ? llvm::Constant::getNullValue(et)
            : value.codegen_expected(ctx, elem);
        v = coerceToType(v, et, b);

        llvm::Value* data = b.CreateLoad(b.getPtrTy(), base, "list.data");
        b.CreateStore(v, b.CreateGEP(et, data, idx, "list.elem"));
    }

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

    llvm::Value* codegen_addr(CodegenContext& ctx, std::string& type_out) const override
    {
        auto& b = ctx.builder;
        llvm::Value* base = target->codegen(ctx);
        std::string t = type_of(ctx, base);

        if (t.starts_with("dict<"))
        {
            auto [k, v] = dict_kv(t);
            if (!is_struct(v)) return nullptr;
            emit_null_check(ctx, base, "Null reference: dict index");

            type_out = v;
            llvm::Type* KT = type_to_llvm(k, b);
            llvm::Value* key = coerceToType(index->codegen_expected(ctx, k), KT, b);
            auto fn = ctx.module.getOrInsertFunction("fab_sdict_ref_" + k,
                llvm::FunctionType::get(b.getPtrTy(), {b.getPtrTy(), KT}, false));
            llvm::Value* p = b.CreateCall(fn, {base, key});
            emit_fail_if(ctx, b.CreateIsNull(p), "Key not found in dict");
            return p;
        }

        if (!t.starts_with("list<")) return nullptr;

        type_out = list_elem(t);
        llvm::Type* et = type_to_llvm(type_out, b);
        llvm::Value* idx = coerceToType(index->codegen(ctx), b.getInt64Ty(), b);

        emit_null_check(ctx, base, "Null reference: list index");
        emit_list_bounds(ctx, base, idx);

        llvm::Value* data = b.CreateLoad(b.getPtrTy(), base, "list.data");
        return b.CreateGEP(et, data, idx, "list.elem");
    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;

        llvm::Value* base = target->codegen(ctx);
        std::string t = type_of(ctx, base);

        if (t.starts_with("dict<"))
        {
            auto [k, v] = dict_kv(t);
            emit_null_check(ctx, base, "Null reference: dict index");
            llvm::Value* kv = index->codegen_expected(ctx, k);
            if (is_struct(v)) return emit_struct_dict_method(ctx, base, t, "get", {kv});
            StdlibFunctionInfo info{"fab_dict_get_" + k + "_" + list_sym(v), v, {t, k}};
            return FunctionalExpression::emit_stdlib_call(ctx, info, {base, kv});
        }

        llvm::Value* idx = coerceToType(index->codegen(ctx), b.getInt64Ty(), b);

        if (t == "string")
        {
            llvm::Value* p = b.CreateGEP(b.getInt8Ty(), base, idx);
            return b.CreateLoad(b.getInt8Ty(), p);
        }

        if (!t.starts_with("list<"))
            throw std::runtime_error("Type '" + t + "' does not support indexing!");

        emit_null_check(ctx, base, "Null reference: list index");
        emit_list_bounds(ctx, base, idx);

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
        ie->assign(ctx, *value);
    }
};
