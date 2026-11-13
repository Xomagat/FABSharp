//
// Created by Xomagat on 13.08.2026.
//

#pragma once
#include "FunctionalExpression.h"
#include "Expression.h"
#include "Statement.h"

#include "NamedCall.h"

class WhileStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> while_statement;

public:
    explicit WhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> while_statement) : condition(std::move(condition)), while_statement(std::move(while_statement)) {}

    void codegen(CodegenContext &context) const override
    {
        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context.context, "loopcond", function);
        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context.context, "loopbody", function);
        llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(context.context, "loopend", function);

        context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(condBB);
        llvm::Value* condVal = condition->codegen(context);
        if (!condVal->getType()->isIntegerTy(1))
            condVal = context.builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "loopcondval");
        context.builder.CreateCondBr(condVal, bodyBB, endBB);

        context.builder.SetInsertPoint(bodyBB);
        context.loop_stack.push_back({condBB, endBB});
        while_statement->codegen(context);
        context.loop_stack.pop_back();

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(endBB);
    }
};

class DoWhileStatement : public Statement
{
private:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> while_statement;

public:
    explicit DoWhileStatement(std::unique_ptr<Expression> condition, std::unique_ptr<Statement> while_statement) : condition(std::move(condition)), while_statement(std::move(while_statement)) {}

    void codegen(CodegenContext& context) const override
    {
        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context.context, "loopbody", function);
        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context.context, "loopcond", function);
        llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(context.context, "loopend", function);

        context.builder.CreateBr(bodyBB);

        context.builder.SetInsertPoint(bodyBB);
        context.loop_stack.push_back({condBB, endBB});
        while_statement->codegen(context);
        context.loop_stack.pop_back();

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(condBB);
        llvm::Value* condVal = condition->codegen(context);
        if (!condVal->getType()->isIntegerTy(1))
            condVal = context.builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "dowhilecondval");
        context.builder.CreateCondBr(condVal, bodyBB, endBB);

        context.builder.SetInsertPoint(endBB);
    }
};

class ForStatement : public Statement
{
private:
    std::unique_ptr<Statement> initialization;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> increment;
    std::unique_ptr<Statement> for_statement;

public:
    explicit ForStatement(std::unique_ptr<Statement> initialization, std::unique_ptr<Expression> condition,
        std::unique_ptr<Statement> increment, std::unique_ptr<Statement> for_statement) : initialization(std::move(initialization)),
                                                                                          condition(std::move(condition)),
                                                                                          increment(std::move(increment)),
                                                                                          for_statement(std::move(for_statement)) {}

    void codegen(CodegenContext& context) const override
    {
        if (initialization)
            initialization->codegen(context);

        llvm::Function* function = context.builder.GetInsertBlock()->getParent();

        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context.context, "loopcond", function);
        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context.context, "loopbody", function);
        llvm::BasicBlock* incrBB = llvm::BasicBlock::Create(context.context, "loopincr", function);
        llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(context.context, "loopend", function);

        context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(condBB);
        if (condition)
        {
            llvm::Value* condVal = condition->codegen(context);
            if (!condVal->getType()->isIntegerTy(1))
                condVal = context.builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "forcondval");
            context.builder.CreateCondBr(condVal, bodyBB, endBB);
        }
        else
            context.builder.CreateBr(bodyBB);

        context.builder.SetInsertPoint(bodyBB);
        context.loop_stack.push_back({incrBB, endBB});
        for_statement->codegen(context);
        context.loop_stack.pop_back();

        if (!context.builder.GetInsertBlock()->getTerminator())
            context.builder.CreateBr(incrBB);

        context.builder.SetInsertPoint(incrBB);
        if (increment)
            increment->codegen(context);
        context.builder.CreateBr(condBB);

        context.builder.SetInsertPoint(endBB);
    }
};

class ForEachStatement : public Statement
{
private:
    std::string type, name;
    std::unique_ptr<Expression> coll_expr;
    std::unique_ptr<Statement> body;

public:
    ForEachStatement(std::string type, std::string name,
                     std::unique_ptr<Expression> coll_expr, std::unique_ptr<Statement> body)
        : type(std::move(type)), name(std::move(name)),
          coll_expr(std::move(coll_expr)), body(std::move(body)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        llvm::Type* i64 = b.getInt64Ty();

        llvm::Value* coll = coll_expr->codegen(ctx);
        std::string ct = type_of(ctx, coll);

        if (ct.starts_with("dict<"))
        {
            auto [k, v] = dict_kv(ct);
            emit_null_check(ctx, coll, "Null reference: foreach over dict");
            StdlibFunctionInfo info{"fab_dict_keys_" + k + "_" + v, "list<" + k + ">", {ct}};
            coll = FunctionalExpression::emit_stdlib_call(ctx, info, {coll});
            ct = "list<" + k + ">";
        }

        bool is_str = (ct == "string");
        if (!is_str && !ct.starts_with("list<"))
            throw std::runtime_error("foreach works only with list, dict and string, not '" + ct + "'!");

        if (!is_str) emit_null_check(ctx, coll, "Null reference: foreach over list");

        std::string elem = is_str ? "char" : list_elem(ct);
        std::string vt   = (type == "auto") ? elem : type;

        bool ok = vt == elem
               || (numeric_rank(vt) && numeric_rank(elem))
               || (elem == "char" && numeric_rank(vt) >= 2);
        if (!ok)
            throw std::runtime_error("foreach: cannot convert element type '" + elem + "' to '" + vt + "'!");

        llvm::Type* elt = type_to_llvm(elem, b);
        llvm::Type* vlt = type_to_llvm(vt, b);

        llvm::AllocaInst* idx = entry_alloca(b, i64, "foreach.i");
        b.CreateStore(b.getInt64(0), idx);

        llvm::Value* str_len = nullptr;
        if (is_str)
        {
            StdlibFunctionInfo li{"fab_length", "int", {"string"}};
            str_len = b.CreateSExt(FunctionalExpression::emit_stdlib_call(ctx, li, {coll}), i64);
        }

        auto save_vars  = ctx.variables;
        auto save_types = ctx.var_types;
        auto save_const = ctx.const_vars;

        llvm::AllocaInst* var = entry_alloca(b, vlt, name);
        ctx.variables[name] = var;
        ctx.var_types[name] = vt;
        ctx.const_vars.erase(name);

        llvm::Function* fn = b.GetInsertBlock()->getParent();
        auto* condBB = llvm::BasicBlock::Create(ctx.context, "feach.cond", fn);
        auto* bodyBB = llvm::BasicBlock::Create(ctx.context, "feach.body", fn);
        auto* incrBB = llvm::BasicBlock::Create(ctx.context, "feach.incr", fn);
        auto* endBB  = llvm::BasicBlock::Create(ctx.context, "feach.end",  fn);

        b.CreateBr(condBB);

        b.SetInsertPoint(condBB);
        llvm::Value* len = is_str
            ? str_len
            : b.CreateZExt(b.CreateLoad(b.getInt32Ty(),
                              b.CreateGEP(b.getInt8Ty(), coll, b.getInt64(8))), i64);
        llvm::Value* cur = b.CreateLoad(i64, idx);
        b.CreateCondBr(b.CreateICmpULT(cur, len), bodyBB, endBB);

        b.SetInsertPoint(bodyBB);
        llvm::Value* i = b.CreateLoad(i64, idx);
        llvm::Value* v;
        if (is_str)
            v = b.CreateLoad(b.getInt8Ty(), b.CreateGEP(b.getInt8Ty(), coll, i));
        else
        {
            llvm::Value* data = b.CreateLoad(b.getPtrTy(), coll, "list.data");
            v = b.CreateLoad(elt, b.CreateGEP(elt, data, i));
        }
        ctx.value_types[v] = elem;
        b.CreateStore(coerceToType(v, vlt, b), var);

        ctx.loop_stack.push_back({incrBB, endBB});
        body->codegen(ctx);
        ctx.loop_stack.pop_back();

        if (!b.GetInsertBlock()->getTerminator())
            b.CreateBr(incrBB);

        b.SetInsertPoint(incrBB);
        b.CreateStore(b.CreateAdd(b.CreateLoad(i64, idx), b.getInt64(1)), idx);
        b.CreateBr(condBB);

        b.SetInsertPoint(endBB);

        ctx.variables  = save_vars;
        ctx.var_types  = save_types;
        ctx.const_vars = save_const;
    }
};