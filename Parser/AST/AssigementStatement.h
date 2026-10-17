//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "../../libs/STDLibInfo.h"

#include "Statement.h"
#include "Expression.h"

inline llvm::Value* to_bool(llvm::Value* v, llvm::IRBuilder<>& b)
{
    llvm::Type* t = v->getType();
    if (t->isIntegerTy(1)) return v;
    if (t->isFloatingPointTy())
        return b.CreateFCmpUNE(v, llvm::ConstantFP::get(t, 0.0));
    return b.CreateIsNotNull(v);
}

inline llvm::Value* coerceToType(llvm::Value* val, llvm::Type* targetType, llvm::IRBuilder<>& builder)
{
    llvm::Type* srcType = val->getType();
    if (srcType == targetType) return val;

    if (srcType->isIntegerTy() && targetType->isFloatingPointTy())
        return builder.CreateSIToFP(val, targetType);
    if (srcType->isFloatingPointTy() && targetType->isIntegerTy())
        return builder.CreateFPToSI(val, targetType);
    if (srcType->isFloatingPointTy() && targetType->isFloatingPointTy())
        return targetType->isDoubleTy()
            ? builder.CreateFPExt(val, targetType)
            : builder.CreateFPTrunc(val, targetType);
    if (srcType->isIntegerTy() && targetType->isIntegerTy())
        return builder.CreateIntCast(val, targetType, true);

    throw std::runtime_error("Cannot coerce between these types in codegen!");
}

inline std::string list_elem(const std::string& t)
{
    return t.substr(5, t.size() - 6);
}

inline llvm::Value* emit_list_new(CodegenContext& ctx, const std::string& list_type)
{
    auto& b = ctx.builder;
    llvm::Type* ptr = b.getInt8Ty()->getPointerTo();
    auto fn = ctx.module.getOrInsertFunction(
        "fab_list_new_" + list_elem(list_type),
        llvm::FunctionType::get(ptr, {}, false));
    llvm::Value* v = b.CreateCall(fn);
    ctx.value_types[v] = list_type;
    return v;
}

inline std::pair<std::string, std::string> dict_kv(const std::string& t)
{
    std::string in = t.substr(5, t.size() - 6);
    int depth = 0;
    for (size_t i = 0; i < in.size(); ++i)
    {
        if      (in[i] == '<') depth++;
        else if (in[i] == '>') depth--;
        else if (in[i] == ',' && depth == 0)
            return {in.substr(0, i), in.substr(i + 1)};
    }
    throw std::runtime_error("Bad dict type: " + t);
}

inline llvm::Value* emit_container_new(CodegenContext& ctx, const std::string& t)
{
    if (t.starts_with("list<")) return emit_list_new(ctx, t);

    auto [k, v] = dict_kv(t);
    auto& b = ctx.builder;
    auto fn = ctx.module.getOrInsertFunction(
        "fab_dict_new_" + k + "_" + v,
        llvm::FunctionType::get(b.getPtrTy(), {}, false));
    llvm::Value* r = b.CreateCall(fn);
    ctx.value_types[r] = t;
    return r;
}

inline bool is_container(const std::string& t)
{
    return t.starts_with("list<") || t.starts_with("dict<");
}

inline bool emit_container_print(CodegenContext& ctx, llvm::Value* val, const std::string& t)
{
    auto& b = ctx.builder;
    std::string sym;
    if      (t.starts_with("list<")) sym = "fab_list_print_" + list_elem(t);
    else if (t.starts_with("dict<")) { auto [k, v] = dict_kv(t); sym = "fab_dict_print_" + k + "_" + v; }
    else return false;

    auto fn = ctx.module.getOrInsertFunction(sym,
        llvm::FunctionType::get(b.getVoidTy(), {b.getPtrTy()}, false));
    b.CreateCall(fn, {val});
    return true;
}

inline bool is_class(const std::string& t) { return class_names.contains(t); }

inline ClassInfo& get_class(CodegenContext& ctx, const std::string& n)
{
    auto it = ctx.classes.find(n);
    if (it == ctx.classes.end()) throw std::runtime_error("Unknown class '" + n + "'!");
    return it->second;
}

inline void emit_null_check(CodegenContext& ctx, llvm::Value* obj, const std::string& msg)
{
    auto& b = ctx.builder;
    llvm::Function* fn = b.GetInsertBlock()->getParent();
    auto* bad = llvm::BasicBlock::Create(ctx.context, "null.bad", fn);
    auto* ok  = llvm::BasicBlock::Create(ctx.context, "null.ok", fn);

    b.CreateCondBr(b.CreateIsNull(obj), bad, ok);

    b.SetInsertPoint(bad);
    auto putsFn = ctx.module.getOrInsertFunction("puts",
        llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, false));
    auto exitFn = ctx.module.getOrInsertFunction("exit",
        llvm::FunctionType::get(b.getVoidTy(), {b.getInt32Ty()}, false));
    b.CreateCall(putsFn, {b.CreateGlobalStringPtr(msg)});
    b.CreateCall(exitFn, {b.getInt32(1)});
    b.CreateUnreachable();

    b.SetInsertPoint(ok);
}

inline llvm::Value* emit_field_ptr(CodegenContext& ctx, llvm::Value* obj, const std::string& cls,
                                   const std::string& field, std::string& type_out)
{
    auto& ci = get_class(ctx, cls);
    int i = ci.field_index(field);
    if (i < 0)
        throw std::runtime_error("Class '" + cls + "' has no field '" + field + "'!");
    if (!ci.fields[i].is_public && ctx.current_class != cls)
        throw std::runtime_error("Field '" + field + "' of class '" + cls + "' is private!");

    type_out = ci.fields[i].type;
    emit_null_check(ctx, obj, "Null reference: field '" + field + "' of " + cls);
    return ctx.builder.CreateStructGEP(ci.st, obj, i + 1, field);
}

inline llvm::Value* emit_this_field_ptr(CodegenContext& ctx, const std::string& name, std::string& ftype)
{
    auto self = ctx.variables.find("this");
    if (self == ctx.variables.end()) return nullptr;

    const std::string& cls = ctx.var_types["this"];
    if (get_class(ctx, cls).field_index(name) < 0) return nullptr;

    llvm::Value* obj = ctx.builder.CreateLoad(ctx.builder.getPtrTy(), self->second);
    return emit_field_ptr(ctx, obj, cls, name, ftype);
}

inline llvm::Value* emit_object_alloc(CodegenContext& ctx, const std::string& cls)
{
    auto& b = ctx.builder;
    auto& cl = get_class(ctx, cls);

    auto* nullp = llvm::ConstantPointerNull::get(b.getPtrTy());
    auto* size = b.CreatePtrToInt(b.CreateGEP(cl.st, nullp, {b.getInt32(1)}), b.getInt64Ty());

    auto callocFn = ctx.module.getOrInsertFunction("calloc",
        llvm::FunctionType::get(b.getPtrTy(), {b.getInt64Ty(), b.getInt64Ty()}, false));
    llvm::Value* obj = b.CreateCall(callocFn, {b.getInt64(1), size});
    ctx.value_types[obj] = cls;
    b.CreateStore(cl.vtable, b.CreateStructGEP(cl.st, obj, 0));

    for (size_t i = 0; i < cl.fields.size(); ++i)
    {
        auto& f = cl.fields[i];
        llvm::Value* init = nullptr;
        if (is_container(f.type))    init = emit_container_new(ctx, f.type);
        else if (f.type == "string") init = b.CreateGlobalStringPtr("");
        if (init) b.CreateStore(init, b.CreateStructGEP(cl.st, obj, (unsigned)i + 1));
    }
    return obj;
}

inline void emit_free_container(CodegenContext& ctx, llvm::Value* v, const std::string& t)
{
    auto& b = ctx.builder;
    std::string sym;
    if (t.starts_with("list<")) sym = "fab_list_free_" + list_elem(t);
    else { auto [k, val] = dict_kv(t); sym = "fab_dict_free_" + k + "_" + val; }

    auto fn = ctx.module.getOrInsertFunction(sym,
        llvm::FunctionType::get(b.getVoidTy(), {b.getPtrTy()}, false));
    b.CreateCall(fn, {v});
}

inline llvm::Type* type_to_llvm(const std::string& type, llvm::IRBuilder<>& builder)
{
    if (type == "void")     return builder.getVoidTy();
    if (type == "int")      return builder.getInt32Ty();
    if (type == "short")    return builder.getInt16Ty();
    if (type == "long")     return builder.getInt64Ty();
    if (type == "char")     return builder.getInt8Ty();
    if (type == "double")   return builder.getDoubleTy();
    if (type == "float")    return builder.getFloatTy();
    if (type == "bool")     return builder.getInt1Ty();
    if (type == "string")   return builder.getInt8Ty()->getPointerTo();
    if (is_container(type)) return builder.getPtrTy();
    if (is_class(type))     return builder.getPtrTy();

    throw std::runtime_error("Unknown type for codegen: " + type);
}

inline std::string llvm_to_type(llvm::Type* t)
{
    if (t->isVoidTy())                 return "void";
    if (t->isIntegerTy(1))      return "bool";
    if (t->isIntegerTy(8))      return "char";
    if (t->isIntegerTy(16))     return "short";
    if (t->isIntegerTy(32))     return "int";
    if (t->isIntegerTy(64))     return "long";
    if (t->isFloatTy())                return "float";
    if (t->isDoubleTy())               return "double";
    if (t->isPointerTy())              return "string";

    throw std::runtime_error("Cannot demangle llvm type for overload resolution!");
}

inline llvm::Value* emit_stdlib_const(CodegenContext& ctx, const StdlibVarInfo& v)
{
    auto& b = ctx.builder;
    llvm::Type* t = type_to_llvm(v.type, b);
    std::string s = v.value;

    if (v.type == "string" || v.type == "char")
    {
        if (s.size() >= 2 && (s.front() == '"' || s.front() == '\''))
            s = s.substr(1, s.size() - 2);

        std::string text;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '\\' && i + 1 < s.size())
            {
                char c = s[++i];
                text.push_back(c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c);
            }
            else text.push_back(s[i]);
        }

        if (v.type == "string") return b.CreateGlobalStringPtr(text);
        return llvm::ConstantInt::get(t, text.empty() ? 0 : (unsigned char)text[0]);
    }
    if (v.type == "bool")
        return b.getInt1(s == "true" || s == "1");
    if (t->isFloatingPointTy())
        return llvm::ConstantFP::get(t, std::stod(s));
    if (t->isIntegerTy())
        return llvm::ConstantInt::get(t, std::stoll(s), true);

    throw std::runtime_error("Unsupported library const type: " + v.type);
}

inline llvm::AllocaInst* entry_alloca(llvm::IRBuilder<>& b, llvm::Type* t, const std::string& name)
{
    llvm::Function* fn = b.GetInsertBlock()->getParent();
    llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return tmp.CreateAlloca(t, nullptr, name);
}

inline llvm::GlobalVariable* stdlib_global(CodegenContext& ctx, const StdlibVarInfo& v)
{
    llvm::Type* t = type_to_llvm(v.type, ctx.builder);
    return llvm::cast<llvm::GlobalVariable>(ctx.module.getOrInsertGlobal(v.symbol, t));
}

inline std::string type_of(CodegenContext& ctx, llvm::Value* v)
{
    auto it = ctx.value_types.find(v);
    if (it != ctx.value_types.end()) return it->second;
    return llvm_to_type(v->getType());
}

class AssigementStatement : public Statement
{
private:
    std::string type, name;
    std::unique_ptr<Expression> expression;
    bool is_const;

public:
    explicit AssigementStatement(std::string type, std::string name, std::unique_ptr<Expression> expr, bool is_const = false)
    : type(std::move(type)), name(std::move(name)), expression(std::move(expr)), is_const(is_const) {}

    void codegen(CodegenContext &context) const override
    {
        if (type == "void")
            throw std::runtime_error("Cannot declare a variable of type 'void'!");

        if (type.empty())
        {
            if (context.const_vars.contains(name))
                throw std::runtime_error("Cannot assign to const variable '" + name + "'!");

            auto it = context.variables.find(name);
            if (it == context.variables.end())
            {
                std::string ft;
                if (auto* p = emit_this_field_ptr(context, name, ft))
                {
                    auto* v = context.builder.CreateLoad(type_to_llvm(ft, context.builder), p, name);
                    context.value_types[v] = ft;
                    return;
                }
                auto lib = stdlib_vars.find(name);
                if (lib == stdlib_vars.end())
                    throw std::runtime_error("Variable " + name + " not found!");
                if (lib->second.is_const)
                    throw std::runtime_error("Cannot assign to library const '" + name + "'!");

                auto* g = stdlib_global(context, lib->second);
                llvm::Value* val = expression->codegen_expected(context, lib->second.type);
                val = coerceToType(val, g->getValueType(), context.builder);
                context.builder.CreateStore(val, g);
                return;
            }

            llvm::Value* val = expression->is_null_literal() ? llvm::Constant::getNullValue(type_to_llvm(type, context.builder)) : expression->codegen_expected(context, context.var_types[name]);
            val = coerceToType(val, it->second->getAllocatedType(), context.builder);
            context.builder.CreateStore(val, it->second);
            return;
        }

        llvm::Type* llvmType = type_to_llvm(type, context.builder);

        llvm::AllocaInst* alloc = entry_alloca(context.builder, llvmType, name);
        context.variables[name] = alloc;
        context.var_types[name] = type;

        if (!expression && is_class(type))
        {
            llvm::Value* obj = emit_object_alloc(context, type);
            auto& ci = get_class(context, type);
            if (ci.ctor && ci.ctor_args.empty())
                context.builder.CreateCall(ci.ctor, {obj});
            context.builder.CreateStore(obj, alloc);
        }
        else if (expression)
        {
            llvm::Value* val;
            if (expression->is_null_literal())
                val = is_container(type)
                    ? emit_container_new(context, type)
                    : llvm::Constant::getNullValue(llvmType);
            else
                val = expression->codegen_expected(context, type);

            val = coerceToType(val, llvmType, context.builder);
            context.builder.CreateStore(val, alloc);
        }

        if (is_const)
            context.const_vars.insert(name);
    }
};