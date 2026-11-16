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

inline int exc_type_id(const std::string& t)
{
    static std::unordered_map<std::string, int> ids;
    return ids.try_emplace(t, (int)ids.size() + 1).first->second;
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

class ThrowStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit ThrowStatement(std::unique_ptr<Expression> e) : expr(std::move(e)) {}

    void codegen(CodegenContext& context) const override
    {
        auto& b = context.builder;

        if (expr->is_null_literal())
            throw std::runtime_error("Cannot throw null!");

        llvm::Value* v = expr->codegen(context);
        std::string t = type_of(context, v);
        if (t == "void")
            throw std::runtime_error("Cannot throw a void value!");

        auto mallocFn = context.module.getOrInsertFunction("malloc",
            llvm::FunctionType::get(b.getPtrTy(), {b.getInt64Ty()}, false));
        llvm::Value* payload = b.CreateCall(mallocFn,
            {emit_size_of(context, v->getType())}, "exc.payload");
        b.CreateStore(v, payload);

        llvm::Value* typeId = b.getInt32(exc_type_id(t));
        llvm::Value* msg;

        if (t == "string")
            msg = v;
        else if (const StdlibFunctionInfo* info = find_lib_method(t, "to_str", {}))
            msg = FunctionalExpression::emit_stdlib_call(context, *info, {v});
        else
            msg = context.builder.CreateGlobalStringPtr("");

        auto throwFn = context.module.getOrInsertFunction("fab_throw",
            llvm::FunctionType::get(b.getVoidTy(),
                {b.getInt32Ty(), b.getPtrTy(), b.getPtrTy()}, false));
        if (auto* f = llvm::dyn_cast<llvm::Function>(throwFn.getCallee()))
        {
            f->setDoesNotReturn();
            f->addFnAttr(llvm::Attribute::Cold);
        }

        auto* call = b.CreateCall(throwFn, {typeId, payload, msg});
        call->setDoesNotReturn();
        b.CreateUnreachable();
    }
};

class CastExpression : public Expression
{
private:
    std::string type;
    std::unique_ptr<Expression> expr;

public:
    explicit CastExpression(std::string type, std::unique_ptr<Expression> e)
        : type(std::move(type)), expr(std::move(e)) {}

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        if (type == "void" || type == "auto")
            throw std::runtime_error("Cannot convert to " + type + " type!");

        auto& b = ctx.builder;
        llvm::Type* dst = type_to_llvm(type, b);

        if (expr->is_null_literal())
        {
            if (!dst->isPointerTy())
                throw std::runtime_error("Cannot cast null to non-pointer type '" + type + "'!");
            llvm::Value* n = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(dst));
            ctx.value_types[n] = type;
            return n;
        }

        llvm::Value* val = expr->codegen(ctx);
        std::string src  = type_of(ctx, val);
        llvm::Type* st   = val->getType();

        if (src == "void")
            throw std::runtime_error("Cannot cast a void value!");

        if (src == type) return val;

        bool dst_num = dst->isIntegerTy() || dst->isFloatingPointTy();
        bool src_num = st->isIntegerTy()  || st->isFloatingPointTy();

        llvm::Value* r = nullptr;

        if (type == "string")
        {
            const StdlibFunctionInfo* info = find_lib_method(src, "to_str", {});
            if (!info)
                throw std::runtime_error("Cannot cast '" + src + "' to string!");
            r = FunctionalExpression::emit_stdlib_call(ctx, *info, {val});
        }
        else if (src == "string" && dst_num)
        {
            std::string m = type == "bool" ? "to_bool"
                          : type == "long" ? "to_long"
                          : (type == "float" || type == "double" || type == "ldouble") ? "to_double"
                          : "to_int";                       // char, short, int
            const StdlibFunctionInfo* info = find_lib_method("string", m, {});
            if (!info)
                throw std::runtime_error("Cannot cast string to '" + type + "'!");
            llvm::Value* v = FunctionalExpression::emit_stdlib_call(ctx, *info, {val});
            r = coerceToType(v, dst, b);
        }
        else if (dst_num && src_num)
        {
            r = coerceToType(val, dst, b);
        }
        else if (dst->isIntegerTy(1))
        {
            r = to_bool(val, b);
        }
        else
            throw std::runtime_error("Cannot cast '" + src + "' to '" + type + "'!");

        ctx.value_types[r] = type;
        return r;
    }

    std::string to_str() const override { return "cast<" + type + ">(" + expr->to_str() + ")"; }
};