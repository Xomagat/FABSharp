//
// Created by Xomagat on 13.10.2026.
//

#pragma once
#include <optional>
#include "FunctionStatement.h"
#include "AssigementStatement.h"

struct MethodDecl
{
    std::string ret, name;
    std::vector<std::string> arg_types, arg_names;
    std::shared_ptr<Statement> body;
    bool is_public;
};

class ClassDefineStatement : public Statement
{
    std::string name;
    std::vector<FieldInfo> fields;
    std::vector<MethodDecl> methods;
    std::optional<MethodDecl> ctor;

public:
    explicit ClassDefineStatement(std::string name) : name(std::move(name)) {}
    void add_field(FieldInfo f)   { fields.push_back(std::move(f)); }
    void add_method(MethodDecl m) { methods.push_back(std::move(m)); }
    void set_ctor(MethodDecl m)   { ctor = std::move(m); }

    void declare(CodegenContext& ctx) const
    {
        auto& b = ctx.builder;
        ClassInfo ci;
        ci.st = llvm::StructType::create(ctx.context, name);

        std::vector<llvm::Type*> ft;
        for (auto& f : fields) ft.push_back(type_to_llvm(f.type, b));
        ci.st->setBody(ft);
        ci.fields = fields;

        auto make = [&](const MethodDecl& m, const std::string& sym) {
            std::vector<llvm::Type*> p{b.getPtrTy()};
            for (auto& t : m.arg_types) p.push_back(type_to_llvm(t, b));
            auto* fty = llvm::FunctionType::get(type_to_llvm(m.ret, b), p, false);
            return llvm::Function::Create(fty, llvm::Function::ExternalLinkage, sym, ctx.module);
        };

        if (ctor) { ci.ctor = make(*ctor, name + ".new"); ci.ctor_args = ctor->arg_types; }
        for (auto& m : methods)
        {
            auto key = mangle_name(m.name, m.arg_types);
            ci.methods[key] = {make(m, name + "." + key), m.ret, m.arg_types, m.is_public};
        }
        ctx.classes[name] = std::move(ci);
    }

    void codegen(CodegenContext& ctx) const override
    {
        auto& ci = ctx.classes.at(name);
        if (ctor)
            emit_function_body(ctx, ci.ctor, name, ctor->arg_names, ctor->arg_types, "void", *ctor->body);
        for (auto& m : methods)
            emit_function_body(ctx, ci.methods.at(mangle_name(m.name, m.arg_types)).fn, name,
                               m.arg_names, m.arg_types, m.ret, *m.body);
    }
};

class NewExpression : public Expression
{
    std::string cls;
    std::vector<std::unique_ptr<Expression>> args;

public:
    explicit NewExpression(std::string cls) : cls(std::move(cls)) {}
    void add_arg(std::unique_ptr<Expression> a) { args.push_back(std::move(a)); }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        auto& ci = get_class(ctx, cls);

        auto* nullp = llvm::ConstantPointerNull::get(b.getPtrTy());
        auto* size = b.CreatePtrToInt(b.CreateGEP(ci.st, nullp, {b.getInt32(1)}), b.getInt64Ty());

        auto callocFn = ctx.module.getOrInsertFunction("calloc",
            llvm::FunctionType::get(b.getPtrTy(), {b.getInt64Ty(), b.getInt64Ty()}, false));
        llvm::Value* obj = emit_object_alloc(ctx, cls);
        ctx.value_types[obj] = cls;

        for (size_t i = 0; i < ci.fields.size(); ++i)
        {
            auto& f = ci.fields[i];
            llvm::Value* init = nullptr;
            if (is_container(f.type))    init = emit_container_new(ctx, f.type);
            else if (f.type == "string") init = b.CreateGlobalStringPtr("");
            if (init) b.CreateStore(init, b.CreateStructGEP(ci.st, obj, (unsigned)i));
        }

        if (!ci.ctor)
        {
            if (!args.empty()) throw std::runtime_error("Class '" + cls + "' has no constructor!");
            return obj;
        }
        if (args.size() != ci.ctor_args.size())
            throw std::runtime_error("Wrong argument count for constructor of '" + cls + "'!");

        std::vector<llvm::Value*> vals{obj};
        for (size_t i = 0; i < args.size(); ++i)
        {
            llvm::Value* v = args[i]->codegen_expected(ctx, ci.ctor_args[i]);
            vals.push_back(coerceToType(v, type_to_llvm(ci.ctor_args[i], b), b));
        }
        b.CreateCall(ci.ctor, vals);
        return obj;
    }

    std::string to_str() const override { return "new " + cls; }
};

class FieldExpression : public Expression
{
    std::unique_ptr<Expression> target;
    std::string name;

public:
    FieldExpression(std::unique_ptr<Expression> t, std::string n) : target(std::move(t)), name(std::move(n)) {}

    llvm::Value* codegen_ptr(CodegenContext& ctx, std::string& ftype) const
    {
        llvm::Value* obj = target->codegen(ctx);
        std::string cls = type_of(ctx, obj);
        if (!is_class(cls)) throw std::runtime_error("Type '" + cls + "' has no fields!");
        return emit_field_ptr(ctx, obj, cls, name, ftype);
    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        std::string ft;
        llvm::Value* p = codegen_ptr(ctx, ft);
        llvm::Value* v = ctx.builder.CreateLoad(type_to_llvm(ft, ctx.builder), p);
        ctx.value_types[v] = ft;
        return v;
    }

    std::string to_str() const override { return target->to_str() + "." + name; }
};

class FieldAssignStatement : public Statement
{
    std::unique_ptr<Expression> target, value;

public:
    FieldAssignStatement(std::unique_ptr<Expression> t, std::unique_ptr<Expression> v)
        : target(std::move(t)), value(std::move(v)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto* fe = dynamic_cast<const FieldExpression*>(target.get());
        std::string ft;
        llvm::Value* ptr = fe->codegen_ptr(ctx, ft);
        llvm::Type* lt = type_to_llvm(ft, ctx.builder);

        llvm::Value* v = value->is_null_literal() ? llvm::Constant::getNullValue(lt)
                                                  : value->codegen_expected(ctx, ft);
        ctx.builder.CreateStore(coerceToType(v, lt, ctx.builder), ptr);
    }
};

class DeleteStatement : public Statement
{
    std::string name;

public:
    explicit DeleteStatement(std::string n) : name(std::move(n)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;

        llvm::Value* slot = nullptr;
        std::string type;
        auto it = ctx.variables.find(name);
        if (it != ctx.variables.end()) { slot = it->second; type = ctx.var_types[name]; }
        else slot = emit_this_field_ptr(ctx, name, type);
        if (!slot) throw std::runtime_error("Variable {" + name + "} not found!");

        llvm::Value* v = b.CreateLoad(b.getPtrTy(), slot);

        if (is_container(type))
            emit_free_container(ctx, v, type);
        else if (is_class(type))
        {
            auto& ci = get_class(ctx, type);
            llvm::Function* fn = b.GetInsertBlock()->getParent();
            auto* body = llvm::BasicBlock::Create(ctx.context, "del.body", fn);
            auto* done = llvm::BasicBlock::Create(ctx.context, "del.done", fn);

            b.CreateCondBr(b.CreateIsNull(v), done, body);

            b.SetInsertPoint(body);
            for (size_t i = 0; i < ci.fields.size(); ++i)
                if (is_container(ci.fields[i].type))
                {
                    auto* p = b.CreateStructGEP(ci.st, v, (unsigned)i);
                    emit_free_container(ctx, b.CreateLoad(b.getPtrTy(), p), ci.fields[i].type);
                }
            auto freeFn = ctx.module.getOrInsertFunction("free",
                llvm::FunctionType::get(b.getVoidTy(), {b.getPtrTy()}, false));
            b.CreateCall(freeFn, {v});
            b.CreateBr(done);

            b.SetInsertPoint(done);
        }
        else
            throw std::runtime_error("Cannot delete a value of type '" + type + "'!");

        b.CreateStore(llvm::Constant::getNullValue(b.getPtrTy()), slot);
    }
};