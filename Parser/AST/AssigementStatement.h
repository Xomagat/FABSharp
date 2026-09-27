//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include "Statement.h"
#include "Expression.h"

#include "../../libs/Environment.h"
#include "../../libs/Value.h"

#include "../../libs/BooleanValue.h"
#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"
#include "../../libs/NullValue.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

inline std::unordered_map<std::string, std::function<std::unique_ptr<Value>(Value*)>> coercers = {
    {"int", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        int i = std::visit([](auto x) { return static_cast<int>(x); }, n->as_number());
        return std::make_unique<NumberValue>(i);
    }},
    {"double", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        double d = std::visit([](auto x) { return static_cast<double>(x); }, n->as_number());
        return std::make_unique<NumberValue>(d);
    }},
    {"float", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        float f = std::visit([](auto x) { return static_cast<float>(x); }, n->as_number());
        return std::make_unique<NumberValue>(f);
    }},
    {"short", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        short sh = std::visit([](auto x) { return static_cast<short>(x); }, n->as_number());
        return std::make_unique<NumberValue>(sh);
    }},
    {"long", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        long l = std::visit([](auto x) { return static_cast<long>(x); }, n->as_number());
        return std::make_unique<NumberValue>(l);
    }},
    {"byte", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<NumberValue*>(v);
        if (!n) return nullptr;
        char ch = std::visit([](auto x) { return static_cast<char>(x); }, n->as_number());
        return std::make_unique<NumberValue>(ch);
    }},
    {"bool", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<BooleanValue*>(v);
        if (!n) return nullptr;
        bool b = n->as_bool();
        return std::make_unique<BooleanValue>(b);
    }},
    {"string", [](Value* v) -> std::unique_ptr<Value> {
        if (dynamic_cast<NullValue*>(v)) return std::make_unique<NullValue>();
        auto n = dynamic_cast<StringValue*>(v);
        if (!n) return nullptr;
        std::string str = n->as_string();
        return std::make_unique<StringValue>(str);
    }},
};

inline bool match_type(const std::string& type, Value* expr)
{
    if (dynamic_cast<NullValue*>(expr) != nullptr)
        return true;

    std::unordered_map<std::string, bool> match = {
        {"string", dynamic_cast<StringValue*>(expr) != nullptr},
        {"bool",   dynamic_cast<BooleanValue*>(expr) != nullptr},
        {"int",    dynamic_cast<NumberValue*>(expr) != nullptr},
        {"double", dynamic_cast<NumberValue*>(expr) != nullptr},
        {"float",  dynamic_cast<NumberValue*>(expr) != nullptr},
        {"short",  dynamic_cast<NumberValue*>(expr) != nullptr},
        {"long",   dynamic_cast<NumberValue*>(expr) != nullptr},
        {"byte",   dynamic_cast<NumberValue*>(expr) != nullptr},
    };

    return match.at(type);
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

inline llvm::Type* type_to_llvm(const std::string& type, llvm::IRBuilder<>& builder)
{
    if (type == "void")   return builder.getVoidTy();
    if (type == "int")    return builder.getInt32Ty();
    if (type == "short")  return builder.getInt16Ty();
    if (type == "long")   return builder.getInt64Ty();
    if (type == "byte")   return builder.getInt8Ty();
    if (type == "double") return builder.getDoubleTy();
    if (type == "float")  return builder.getFloatTy();
    if (type == "bool")   return builder.getInt1Ty();
    if (type == "string") return builder.getInt8Ty()->getPointerTo();

    throw std::runtime_error("Unknown type for codegen: " + type);
}

inline std::string llvm_to_type(llvm::Type* t)
{
    if (t->isVoidTy())                 return "void";
    if (t->isIntegerTy(1))             return "bool";
    if (t->isIntegerTy(8))             return "byte";
    if (t->isIntegerTy(16))            return "short";
    if (t->isIntegerTy(32))            return "int";
    if (t->isIntegerTy(64))            return "long";
    if (t->isFloatTy())                return "float";
    if (t->isDoubleTy())               return "double";
    if (t->isPointerTy())              return "string";

    throw std::runtime_error("Cannot demangle llvm type for overload resolution!");
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

    void execute(Environment& env) const override
    {
        std::unique_ptr<Value> result;

        if (type == "void")
            throw std::runtime_error("Cannot declare a variable of type 'void'!");

        if (expression)
        {
            result = expression->eval(env);
            if (!type.empty() && !match_type(type, result.get()))
                throw std::runtime_error("Uncorrected expression type!");
        }
        else
        {
            result = std::make_unique<NullValue>();
        }

        if (type.empty())
        {
            if (!env.assign(name, std::move(result)))
                throw std::runtime_error("Variable" + name + " not found!");
            return;
        }

        if (!match_type(type, result.get()))
            throw std::runtime_error("Uncorrected expression type!");

        env.define(type, name, std::move(coercers.at(type)(result.get())));
    }

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
                std::runtime_error("Variable " + name + " not found!");

            llvm::Value* val = expression->is_null_literal() ? llvm::Constant::getNullValue(type_to_llvm(type, context.builder)) : expression->codegen(context);
            val = coerceToType(val, it->second->getAllocatedType(), context.builder);
            context.builder.CreateStore(val, it->second);
            return;
        }

        llvm::Type* llvmType = type_to_llvm(type, context.builder);

        llvm::AllocaInst* alloc = context.builder.CreateAlloca(llvmType, nullptr, name);
        context.variables[name] = alloc;

        if (expression)
        {
            llvm::Value* val = expression->is_null_literal() ? llvm::Constant::getNullValue(llvmType) : expression->codegen(context);
            val = coerceToType(val, llvmType, context.builder);
            context.builder.CreateStore(val, alloc);
        }

        if (is_const)
            context.const_vars.insert(name);
    }
};