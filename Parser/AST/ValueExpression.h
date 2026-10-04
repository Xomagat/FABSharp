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

using Literal = std::variant<long long, long double, bool, std::string, NullTag>;

class ValueExpression : public Expression
{
private:
    Literal value;

public:

    explicit ValueExpression(long long v)   : value(v) {}
    explicit ValueExpression(long double v) : value(v) {}
    explicit ValueExpression(std::string v) : value(std::move(v)) {}
    explicit ValueExpression(BoolTag v)     : value(v.b) {}
    explicit ValueExpression(NullTag)       : value(NullTag{}) {}

    llvm::Value* codegen(CodegenContext& ctx) const override {
        return std::visit(overloaded {
            [&](long long v)   { return (llvm::Value*)llvm::ConstantInt::get(ctx.builder.getInt32Ty(), v); },
            [&](long double v) { return (llvm::Value*)llvm::ConstantFP::get(ctx.builder.getDoubleTy(), (double)v); },
            [&](bool v)        { return (llvm::Value*)ctx.builder.getInt1(v); },
            [&](const std::string& s) { return (llvm::Value*)ctx.builder.CreateGlobalStringPtr(s); },
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