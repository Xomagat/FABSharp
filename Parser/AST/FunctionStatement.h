//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "FunctionalExpression.h"

#include "AssigementStatement.h"
#include "Statement.h"

inline void emit_function_body(CodegenContext& ctx, llvm::Function* fn, const std::string& this_class,
                               const std::vector<std::string>& arg_names,
                               const std::vector<std::string>& arg_types,
                               const std::string& ret, Statement& body)
{
    auto* saved_bb   = ctx.builder.GetInsertBlock();
    auto saved_vars  = ctx.variables;
    auto saved_types = ctx.var_types;
    auto saved_const = ctx.const_vars;
    auto saved_cls   = ctx.current_class;
    auto saved_ret   = ctx.current_ret;

    ctx.variables.clear(); ctx.var_types.clear(); ctx.const_vars.clear();
    ctx.current_class = this_class;
    ctx.current_ret = ret;

    ctx.builder.SetInsertPoint(llvm::BasicBlock::Create(ctx.context, "entry", fn));

    auto arg = fn->arg_begin();
    if (!this_class.empty())
    {
        arg->setName("this");
        auto* a = entry_alloca(ctx.builder, ctx.builder.getPtrTy(), "this");
        ctx.builder.CreateStore(&*arg, a);
        ctx.variables["this"] = a;
        ctx.var_types["this"] = this_class;
        ++arg;
    }
    for (size_t i = 0; i < arg_names.size(); ++i, ++arg)
    {
        arg->setName(arg_names[i]);
        auto* a = entry_alloca(ctx.builder, type_to_llvm(arg_types[i], ctx.builder), arg_names[i]);
        ctx.builder.CreateStore(&*arg, a);
        ctx.variables[arg_names[i]] = a;
        ctx.var_types[arg_names[i]] = arg_types[i];
    }

    body.codegen(ctx);

    if (!ctx.builder.GetInsertBlock()->getTerminator())
    {
        if (ret == "void") ctx.builder.CreateRetVoid();
        else ctx.builder.CreateRet(llvm::Constant::getNullValue(type_to_llvm(ret, ctx.builder)));
    }

    ctx.variables = saved_vars; ctx.var_types = saved_types; ctx.const_vars = saved_const;
    ctx.current_class = saved_cls; ctx.current_ret = saved_ret;
    if (saved_bb) ctx.builder.SetInsertPoint(saved_bb);
}

class FunctionStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit FunctionStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void codegen(CodegenContext &context) const override
    {
        expr->codegen(context);
    }
};

class FunctionDefineStatement : public Statement
{
private:
    std::string type;
    std::string name;
    std::vector<std::string> arg_names;
    std::vector<std::string> arg_types;
    std::shared_ptr<Statement> body;
    std::vector<std::shared_ptr<Expression>> defaults;

public:
    explicit FunctionDefineStatement(std::string& type, std::string& name, std::vector<std::string>& arg_types, std::vector<std::string>& arg_names, std::unique_ptr<Statement> body)
        : type(type), name(name), arg_types(arg_types), arg_names(arg_names), body(std::move(body)) {}

    void set_defaults(std::vector<std::shared_ptr<Expression>> d) { defaults = std::move(d); }
    auto get_defaults() const { return defaults; }

    void codegen(CodegenContext &context) const override
    {
        std::vector<llvm::Type*> param_types;
        for (auto& arg_type : arg_types)
            param_types.push_back(type_to_llvm(arg_type, context.builder));

        llvm::FunctionType* fn_type = llvm::FunctionType::get(
            type_to_llvm(type, context.builder), param_types, false);

        llvm::Function* function = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, name, context.module);

        context.functions[mangle_name(name, arg_types)] = function;
        context.func_ret[mangle_name(name, arg_types)]  = type;

        size_t first_def = arg_types.size();
        for (size_t i = 0; i < defaults.size(); ++i)
            if (defaults[i]) { first_def = i; break; }

        for (size_t n = first_def; n < arg_types.size(); ++n)
        {
            std::vector<std::string> sub(arg_types.begin(), arg_types.begin() + n);
            auto k = mangle_name(name, sub);
            context.functions[k] = function;
            context.func_ret[k]  = type;
            context.func_defaults[k] = {arg_types, defaults};
        }

        llvm::BasicBlock* entry = llvm::BasicBlock::Create(context.context, "entry", function);
        auto save_insert_block = context.builder.GetInsertBlock();
        context.builder.SetInsertPoint(entry);

        auto save_variables = context.variables;
        auto save_var_types = context.var_types;
        auto save_const     = context.const_vars;
        auto save_ret       = context.current_ret;
        auto save_class     = context.current_class;

        context.current_ret   = type;
        context.current_class = "";

        int i = 0;
        for (auto& arg : function->args())
        {
            arg.setName(arg_names[i]);
            llvm::AllocaInst* alloc = entry_alloca(context.builder, param_types[i], arg_names[i]);
            context.builder.CreateStore(&arg, alloc);
            context.variables[arg_names[i]] = alloc;
            context.var_types[arg_names[i]] = arg_types[i];
            i++;
        }

        body->codegen(context);

        if (!context.builder.GetInsertBlock()->getTerminator())
        {
            if (type == "void")
                context.builder.CreateRetVoid();
            else
                context.builder.CreateRet(llvm::Constant::getNullValue(type_to_llvm(type, context.builder)));
        }

        context.variables = save_variables;
        context.var_types = save_var_types;
        context.const_vars    = save_const;
        context.current_ret   = save_ret;
        context.current_class = save_class;
        context.builder.SetInsertPoint(save_insert_block);
    }

    std::string get_type()
    {
        return type;
    }

    std::vector<std::string> get_arg_types()
    {
        return arg_types;
    }

    std::vector<std::string> get_arg_names()
    {
        return arg_names;
    }

    std::shared_ptr<Statement> get_body()
    {
        return body;
    }

    std::string get_name() const
    {
        return name;
    }
};