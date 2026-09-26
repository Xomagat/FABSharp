//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <string>
#include <variant>
#include <iostream>

#include "../../libs/Value.h"
#include "../../CodeGen/CodegenContext.h"

#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"
#include "../../libs/BooleanValue.h"
#include "../../libs/NullValue.h"

#include "Expression.h"

struct NullTag {};
struct BoolTag { bool b; };

class ValueExpression : public Expression
{
private:
    std::unique_ptr<Value> value;

public:
    explicit ValueExpression(std::variant<char, short, int, long, long long, double, long double> value)
    {
        this->value = std::make_unique<NumberValue>(value);
    }
    explicit ValueExpression(std::string value)
    {
        this->value = std::make_unique<StringValue>(value);
    }
    explicit ValueExpression(BoolTag value)
    {
        this->value = std::make_unique<BooleanValue>(value.b);
    }
    explicit ValueExpression(NullTag value)
    {
        this->value = std::make_unique<NullValue>();
    }

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        if (auto null = dynamic_cast<NullValue*>(value.get()))
            return std::make_unique<NullValue>();
        if (auto b = dynamic_cast<BooleanValue*>(value.get()))
            return std::make_unique<BooleanValue>(b->as_bool());
        if (auto s = dynamic_cast<StringValue*>(value.get()))
            return std::make_unique<StringValue>(s->as_string());
        return std::make_unique<NumberValue>(value->as_number());
    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        if (auto s = dynamic_cast<StringValue*>(value.get()))
            return ctx.builder.CreateGlobalStringPtr(s->as_string());

        if (auto b = dynamic_cast<BooleanValue*>(value.get()))
            return llvm::ConstantInt::get(ctx.builder.getInt1Ty(), b->as_bool());

        if (auto n = dynamic_cast<NumberValue*>(value.get()))
        {
            auto num = n->as_number();
            return std::visit([&](auto x) -> llvm::Value* {
                using T = decltype(x);
                if constexpr (std::is_floating_point_v<T>)
                    return llvm::ConstantFP::get(ctx.builder.getDoubleTy(), static_cast<double>(x));
                else
                    return llvm::ConstantInt::get(ctx.builder.getInt32Ty(), static_cast<long long>(x));
            }, num);
        }

        throw std::runtime_error("Codegen for this value type not implemented yet!");
    }

    bool is_null_literal() const override
    {
        return dynamic_cast<NullValue*>(value.get()) != nullptr;
    }

    std::string to_str() const override
    {
        return value->as_string();
    }
};