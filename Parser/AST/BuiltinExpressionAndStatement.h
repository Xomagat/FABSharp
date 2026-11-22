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

inline llvm::Value* emit_rt(CodegenContext& ctx, const char* name, llvm::Type* ret,
                            std::vector<llvm::Type*> pt, std::vector<llvm::Value*> args,
                            bool noreturn = false)
{
    auto& b = ctx.builder;
    auto fn = ctx.module.getOrInsertFunction(name, llvm::FunctionType::get(ret, pt, false));
    if (noreturn)
        if (auto* f = llvm::dyn_cast<llvm::Function>(fn.getCallee())) f->setDoesNotReturn();
    return b.CreateCall(fn, args);
}

inline void emit_try_pop(CodegenContext& ctx)
{
    emit_rt(ctx, "fab_try_pop", ctx.builder.getVoidTy(), {}, {});
}

inline void emit_unwind_to(CodegenContext& ctx, size_t depth)
{
    auto saved = ctx.try_stack;
    for (size_t i = saved.size(); i > depth; --i)
    {
        ctx.try_stack.resize(i - 1);
        emit_try_pop(ctx);
        if (saved[i - 1].fin) saved[i - 1].fin->codegen(ctx);
    }
    ctx.try_stack = saved;
}

constexpr unsigned FAB_TRY_BYTES = 512;

inline llvm::Value* emit_setjmp(CodegenContext& ctx, llvm::Value* rec)
{
    auto& b = ctx.builder;
    llvm::FunctionCallee fn;
    std::vector<llvm::Value*> args{rec};

#ifdef _WIN32
    fn = ctx.module.getOrInsertFunction("_setjmp",
        llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy(), b.getPtrTy()}, false));
    args.push_back(llvm::ConstantPointerNull::get(b.getPtrTy()));
#else
    fn = ctx.module.getOrInsertFunction("_setjmp",
        llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, false));
#endif

    if (auto* f = llvm::dyn_cast<llvm::Function>(fn.getCallee()))
        f->addFnAttr(llvm::Attribute::ReturnsTwice);

    auto* call = b.CreateCall(fn, args);
    call->addFnAttr(llvm::Attribute::ReturnsTwice);
    return call;
}

inline llvm::AllocaInst* emit_try_enter(CodegenContext& ctx, llvm::BasicBlock* normal,
                                        llvm::BasicBlock* thrown)
{
    auto& b = ctx.builder;
    auto* rec = entry_alloca(b, llvm::ArrayType::get(b.getInt8Ty(), FAB_TRY_BYTES), "try.rec");
    rec->setAlignment(llvm::Align(16));

    emit_rt(ctx, "fab_try_push", b.getVoidTy(), {b.getPtrTy()}, {rec});

    llvm::Value* r = emit_setjmp(ctx, rec);
    b.CreateCondBr(b.CreateICmpEQ(r, b.getInt32(0)), normal, thrown);
    return rec;
}

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
            if (st->isFloatingPointTy() && dst->isIntegerTy() && !dst->isIntegerTy(1) && is_unsigned(type))
                r = b.CreateIntrinsic(llvm::Intrinsic::fptoui_sat, {dst, st}, {val});
            else
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

struct CatchClause
{
    std::string type, name;
    std::shared_ptr<Statement> body;
};

class TryStatement : public Statement
{
    std::shared_ptr<Statement> body, fin;
    std::vector<CatchClause> catches;

public:
    TryStatement(std::shared_ptr<Statement> b, std::vector<CatchClause> c, std::shared_ptr<Statement> f)
        : body(std::move(b)), fin(std::move(f)), catches(std::move(c)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        llvm::Function* fn = b.GetInsertBlock()->getParent();
        fn->addFnAttr(llvm::Attribute::NoInline);

        auto mk = [&](const char* n) { return llvm::BasicBlock::Create(ctx.context, n, fn); };
        auto* endBB = mk("try.end");

        llvm::AllocaInst* outer = nullptr;
        llvm::BasicBlock* outerThrown = nullptr;
        if (fin)
        {
            auto* outerBody = mk("try.outer");
            outerThrown = mk("try.fin.throw");
            outer = emit_try_enter(ctx, outerBody, outerThrown);
            b.SetInsertPoint(outerBody);
            ctx.try_stack.push_back({fin.get()});
        }

        if (catches.empty())
            body->codegen(ctx);
        else
        {
            auto* tryBB  = mk("try.body");
            auto* dispBB = mk("try.dispatch");
            auto* after  = mk("try.after");
            llvm::AllocaInst* rec = emit_try_enter(ctx, tryBB, dispBB);

            b.SetInsertPoint(tryBB);
            ctx.try_stack.push_back({nullptr});
            body->codegen(ctx);
            ctx.try_stack.pop_back();
            if (!b.GetInsertBlock()->getTerminator())
            {
                emit_try_pop(ctx);
                b.CreateBr(after);
            }

            b.SetInsertPoint(dispBB);
            std::vector<llvm::BasicBlock*> cbb;
            for (size_t i = 0; i < catches.size(); ++i) cbb.push_back(mk("catch.body"));
            auto* noMatch = mk("catch.nomatch");

            llvm::Value* id = emit_rt(ctx, "fab_exc_type", b.getInt32Ty(), {b.getPtrTy()}, {rec});
            for (size_t i = 0; i < catches.size(); ++i)
            {
                if (catches[i].type.empty()) { b.CreateBr(cbb[i]); break; }
                auto* next = (i + 1 < catches.size()) ? mk("catch.test") : noMatch;
                b.CreateCondBr(b.CreateICmpEQ(id, b.getInt32(exc_type_id(catches[i].type))),
                               cbb[i], next);
                if (next != noMatch) b.SetInsertPoint(next);
            }

            b.SetInsertPoint(noMatch);
            emit_rt(ctx, "fab_rethrow", b.getVoidTy(), {b.getPtrTy()}, {rec}, true);
            b.CreateUnreachable();

            for (size_t i = 0; i < catches.size(); ++i)
            {
                auto& c = catches[i];
                b.SetInsertPoint(cbb[i]);

                auto sv_vars = ctx.variables; auto sv_types = ctx.var_types; auto sv_const = ctx.const_vars;

                std::string vt = c.type.empty() ? "string" : c.type;
                llvm::Value* val;
                if (c.type.empty())
                    val = emit_rt(ctx, "fab_exc_msg", b.getPtrTy(), {b.getPtrTy()}, {rec});
                else
                {
                    llvm::Value* p = emit_rt(ctx, "fab_exc_payload", b.getPtrTy(), {b.getPtrTy()}, {rec});
                    val = b.CreateLoad(type_to_llvm(vt, b), p);
                }
                auto* slot = entry_alloca(b, type_to_llvm(vt, b), c.name);
                b.CreateStore(val, slot);
                ctx.variables[c.name] = slot;
                ctx.var_types[c.name] = vt;
                ctx.const_vars.erase(c.name);

                c.body->codegen(ctx);

                ctx.variables = sv_vars; ctx.var_types = sv_types; ctx.const_vars = sv_const;
                if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(after);
            }
            b.SetInsertPoint(after);
        }

        if (fin)
        {
            ctx.try_stack.pop_back();
            if (!b.GetInsertBlock()->getTerminator())
            {
                emit_try_pop(ctx);
                fin->codegen(ctx);
                if (!b.GetInsertBlock()->getTerminator()) b.CreateBr(endBB);
            }

            b.SetInsertPoint(outerThrown);
            fin->codegen(ctx);
            if (!b.GetInsertBlock()->getTerminator())
            {
                emit_rt(ctx, "fab_rethrow", b.getVoidTy(), {b.getPtrTy()}, {outer}, true);
                b.CreateUnreachable();
            }
        }
        else if (!b.GetInsertBlock()->getTerminator())
            b.CreateBr(endBB);

        b.SetInsertPoint(endBB);
    }
};

class AwaitExpression : public Expression
{
    std::unique_ptr<Expression> target;

public:
    explicit AwaitExpression(std::unique_ptr<Expression> t) : target(std::move(t)) {}

    static llvm::Value* emit_wait(CodegenContext& ctx, const Expression& target, const std::string& want)
    {
        auto& b = ctx.builder;
        if (target.is_null_literal())
            throw std::runtime_error("Cannot await null!");

        llvm::Value* th = target.codegen(ctx);
        std::string t = type_of(ctx, th);
        if (t != "thread")
            throw std::runtime_error("await needs a thread, not '" + t + "'!");

        llvm::Value* w = want.empty()
            ? (llvm::Value*)llvm::ConstantPointerNull::get(b.getPtrTy())
            : b.CreateGlobalStringPtr(want);

        return emit_rt(ctx, "fab_thread_await", b.getPtrTy(),
                       {b.getPtrTy(), b.getPtrTy()}, {th, w});
    }

    llvm::Value* codegen_expected(CodegenContext& ctx, const std::string& expected) const override
    {
        if (expected == "void" || expected == "auto" || is_struct(expected))
            throw std::runtime_error("await: cannot take a result of type '" + expected + "'!");

        llvm::Value* slot = emit_wait(ctx, *target, expected);
        llvm::Value* v = ctx.builder.CreateLoad(type_to_llvm(expected, ctx.builder), slot);
        ctx.value_types[v] = expected;
        return v;
    }

    llvm::Value* codegen(CodegenContext&) const override
    {
        throw std::runtime_error("await needs a type context: int x = await t;");
    }

    std::string to_str() const override { return "await " + target->to_str(); }
};

class AwaitStatement : public Statement
{
    std::unique_ptr<Expression> target;

public:
    explicit AwaitStatement(std::unique_ptr<Expression> t) : target(std::move(t)) {}

    void codegen(CodegenContext& ctx) const override
    {
        AwaitExpression::emit_wait(ctx, *target, "");
    }
};