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
    std::vector<std::shared_ptr<Expression>> base_args;
};

class DtorBody : public Statement
{
    std::shared_ptr<Statement> user;
    std::string cls, parent;
public:
    DtorBody(std::shared_ptr<Statement> u, std::string c, std::string p)
        : user(std::move(u)), cls(std::move(c)), parent(std::move(p)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        if (user) user->codegen(ctx);

        auto& ci = get_class(ctx, cls);
        llvm::Value* self = b.CreateLoad(b.getPtrTy(), ctx.variables["this"]);

        for (size_t i = 0; i < ci.fields.size(); ++i)
        {
            auto& f = ci.fields[i];
            if (f.owner != cls) continue;
            auto* p = b.CreateStructGEP(ci.st, self, (unsigned)i + 1);
            if (is_container(f.type))
                emit_free_container(ctx, b.CreateLoad(b.getPtrTy(), p), f.type);
            else if (is_lib_type(f.type) && !lib_free_symbol(f.type).empty())
                emit_free_lib(ctx, b.CreateLoad(b.getPtrTy(), p), f.type);
        }
        if (!parent.empty())
            b.CreateCall(get_class(ctx, parent).dtor, {self});
    }
};

class CtorBody : public Statement
{
    std::shared_ptr<Statement> user;
    std::string parent;
    std::vector<std::shared_ptr<Expression>> base_args;

public:
    CtorBody(std::shared_ptr<Statement> u, std::string p,
             std::vector<std::shared_ptr<Expression>> a)
        : user(std::move(u)), parent(std::move(p)), base_args(std::move(a)) {}

    void codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;

        if (!parent.empty())
        {
            auto& pc = get_class(ctx, parent);
            if (pc.ctor)
            {
                if (base_args.size() != pc.ctor_args.size())
                    throw std::runtime_error("Constructor of '" + parent + "' takes "
                        + std::to_string(pc.ctor_args.size())
                        + " argument(s): write ': " + parent + "(...)' after the constructor!");

                std::vector<llvm::Value*> vals{b.CreateLoad(b.getPtrTy(), ctx.variables["this"])};
                for (size_t i = 0; i < base_args.size(); ++i)
                    vals.push_back(coerceToType(base_args[i]->codegen_expected(ctx, pc.ctor_args[i]),
                                                type_to_llvm(pc.ctor_args[i], b), b));
                b.CreateCall(pc.ctor, vals);
            }
            else if (!base_args.empty())
                throw std::runtime_error("Class '" + parent + "' has no constructor!");
        }

        user->codegen(ctx);
    }
};

class SuperCallStatement : public Statement
{
    std::vector<std::unique_ptr<Expression>> args;
public:
    void add_arg(std::unique_ptr<Expression> a) { args.push_back(std::move(a)); }

    void codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        auto& cur = get_class(ctx, ctx.current_class);
        if (cur.parent.empty()) throw std::runtime_error("'super' in a class without parent!");

        auto& pc = get_class(ctx, cur.parent);
        if (!pc.ctor || pc.ctor_args.size() != args.size())
            throw std::runtime_error("Wrong super(...) arguments!");

        std::vector<llvm::Value*> vals{b.CreateLoad(b.getPtrTy(), ctx.variables["this"])};
        for (size_t i = 0; i < args.size(); ++i)
            vals.push_back(coerceToType(args[i]->codegen_expected(ctx, pc.ctor_args[i]),
                                        type_to_llvm(pc.ctor_args[i], b), b));
        b.CreateCall(pc.ctor, vals);
    }
};

class ClassDefineStatement : public Statement
{
    std::string name;
    std::string parent;
    std::shared_ptr<Statement> dtor_body;
    std::vector<FieldInfo> fields;
    std::vector<MethodDecl> methods;
    std::optional<MethodDecl> ctor;

public:
    explicit ClassDefineStatement(std::string name, std::string parent = "")
    : name(std::move(name)), parent(std::move(parent)) {}
    void set_dtor(std::shared_ptr<Statement> b) { dtor_body = std::move(b); }
    void add_field(FieldInfo f)   { fields.push_back(std::move(f)); }
    void add_method(MethodDecl m) { methods.push_back(std::move(m)); }
    void set_ctor(MethodDecl m)   { ctor = std::move(m); }

    void declare(CodegenContext& ctx) const
    {
        auto& b = ctx.builder;
        ClassInfo ci;
        ci.st = llvm::StructType::create(ctx.context, name);
        ci.parent = parent;

        if (!parent.empty())
        {
            auto& p = get_class(ctx, parent);
            ci.fields  = p.fields;
            ci.methods = p.methods;
            ci.vslots  = p.vslots;
            ci.ctor = p.ctor; ci.ctor_args = p.ctor_args;
        }
        else ci.vslots.push_back(nullptr);

        for (auto f : fields) { f.owner = name; ci.fields.push_back(f); }

        std::vector<llvm::Type*> ft{b.getPtrTy()};
        for (auto& f : ci.fields) ft.push_back(type_to_llvm(f.type, b));
        ci.st->setBody(ft);

        auto make = [&](const MethodDecl& m, const std::string& sym) {
            std::vector<llvm::Type*> p{b.getPtrTy()};
            for (auto& t : m.arg_types) p.push_back(type_to_llvm(t, b));
            auto* fty = llvm::FunctionType::get(type_to_llvm(m.ret, b), p, false);
            return llvm::Function::Create(fty, llvm::Function::ExternalLinkage, sym, ctx.module);
        };

        if (ctor) { ci.ctor = make(*ctor, name + ".new"); ci.ctor_args = ctor->arg_types; }

        ci.dtor = llvm::Function::Create(
            llvm::FunctionType::get(b.getVoidTy(), {b.getPtrTy()}, false),
            llvm::Function::ExternalLinkage, name + ".dtor", ctx.module);
        ci.vslots[0] = ci.dtor;

        for (auto& m : methods)
        {
            auto key = mangle_name(m.name, m.arg_types);
            auto* fn = make(m, name + "." + key);
            int slot;
            auto it = ci.methods.find(key);
            if (it != ci.methods.end())
            {
                if (it->second.ret != m.ret)
                    throw std::runtime_error("Override of '" + m.name + "' changes return type!");
                slot = it->second.slot;
                ci.vslots[slot] = fn;
            }
            else { slot = (int)ci.vslots.size(); ci.vslots.push_back(fn); }
            ci.methods[key] = {fn, m.ret, m.arg_types, m.is_public, slot};
        }

        auto* arrTy = llvm::ArrayType::get(b.getPtrTy(), ci.vslots.size());
        std::vector<llvm::Constant*> init(ci.vslots.begin(), ci.vslots.end());
        ci.vtable = new llvm::GlobalVariable(ctx.module, arrTy, true,
            llvm::GlobalValue::InternalLinkage, llvm::ConstantArray::get(arrTy, init), name + ".vtable");

        ctx.classes[name] = std::move(ci);
    }

    void codegen(CodegenContext& ctx) const override
    {
        auto& ci = ctx.classes.at(name);

        if (ctor)
        {
            CtorBody cb(ctor->body, parent, ctor->base_args);
            emit_function_body(ctx, ci.ctor, name, ctor->arg_names, ctor->arg_types, "void", cb);
        }
        DtorBody db(dtor_body, name, parent);
        emit_function_body(ctx, ci.dtor, name, {}, {}, "void", db);

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

        llvm::Value* obj = emit_object_alloc(ctx, cls);

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
        else if (is_lib_type(type))
            emit_free_lib(ctx, v, type);
        else if (is_class(type))
        {
            llvm::Function* fn = b.GetInsertBlock()->getParent();
            auto* body = llvm::BasicBlock::Create(ctx.context, "del.body", fn);
            auto* done = llvm::BasicBlock::Create(ctx.context, "del.done", fn);

            b.CreateCondBr(b.CreateIsNull(v), done, body);

            b.SetInsertPoint(body);
            llvm::Value* vptr = b.CreateLoad(b.getPtrTy(), v);
            llvm::Value* dfn  = b.CreateLoad(b.getPtrTy(), vptr);
            auto* fty = llvm::FunctionType::get(b.getVoidTy(), {b.getPtrTy()}, false);
            b.CreateCall(fty, dfn, {v});
            b.CreateCall(ctx.module.getOrInsertFunction("free", fty), {v});
            b.CreateBr(done);

            b.SetInsertPoint(done);
        }
        else
            throw std::runtime_error("Cannot delete a value of type '" + type + "'!");

        b.CreateStore(llvm::Constant::getNullValue(b.getPtrTy()), slot);
    }
};