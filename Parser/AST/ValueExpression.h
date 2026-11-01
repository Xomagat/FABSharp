//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <variant>
#include <iostream>

#include "../../CodeGen/CodegenContext.h"

#include "Expression.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

struct NullTag {};
struct BoolTag { bool b; };

using Literal = std::variant<long long, long double, bool, std::string, char, NullTag>;

class ValueExpression : public Expression
{
private:
    Literal value;
    std::string text;
    std::string num_type;

public:

    explicit ValueExpression(long long v)   : value(v) {}
    explicit ValueExpression(long double v) : value(v) {}
    explicit ValueExpression(std::string v) : value(std::move(v)) {}
    explicit ValueExpression(char v)        : value(v) {}
    explicit ValueExpression(BoolTag v)     : value(v.b) {}
    explicit ValueExpression(NullTag)       : value(NullTag{}) {}
    explicit ValueExpression(long double v, std::string t, std::string ty)
        : value(v), text(std::move(t)), num_type(std::move(ty)) {}

    llvm::Value* typed(CodegenContext& ctx) const
    {
        auto& b = ctx.builder;
        if (num_type == "float")   return llvm::ConstantFP::get(b.getFloatTy(),  llvm::StringRef(text));
        if (num_type == "double")  return llvm::ConstantFP::get(b.getDoubleTy(), llvm::StringRef(text));
        if (num_type == "ldouble") return llvm::ConstantFP::get(llvm::Type::getX86_FP80Ty(ctx.context), llvm::StringRef(text));
        if (num_type == "long")    return llvm::ConstantInt::get(b.getInt64Ty(), std::stoll(text), true);
        if (num_type == "short")   return llvm::ConstantInt::get(b.getInt16Ty(), std::stoll(text), true);
        return nullptr;
    }

    llvm::Value* codegen_expected(CodegenContext& ctx, const std::string& expected) const override
    {
        if (!num_type.empty()) return typed(ctx);
        if (expected == "ldouble" && std::holds_alternative<long double>(value))
            return llvm::ConstantFP::get(llvm::Type::getX86_FP80Ty(ctx.context), llvm::StringRef(text));
        return codegen(ctx);
    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        if (!num_type.empty()) return typed(ctx);
        return std::visit(overloaded {
            [&](long long v) {
                            llvm::Type* t = (v >= INT32_MIN && v <= INT32_MAX)
                                ? ctx.builder.getInt32Ty()
                                : ctx.builder.getInt64Ty();
                            return (llvm::Value*)llvm::ConstantInt::get(t, (uint64_t)v, true);
                         },
            [&](long double v) { return (llvm::Value*)llvm::ConstantFP::get(ctx.builder.getDoubleTy(), (double)v); },
            [&](bool v)        { return (llvm::Value*)ctx.builder.getInt1(v); },
            [&](const std::string& s) { return (llvm::Value*)ctx.builder.CreateGlobalStringPtr(s); },
            [&](const char& c) { return (llvm::Value*)llvm::ConstantInt::get(ctx.builder.getInt8Ty(), c, false); },
            [&](NullTag)       { throw std::runtime_error("null has no standalone value"); return (llvm::Value*)nullptr; }
        }, value);
    }

    bool is_null_literal() const override
    {
        return std::holds_alternative<NullTag>(value);
    }

    std::string to_str() const override
    {
        return "";
    }
};