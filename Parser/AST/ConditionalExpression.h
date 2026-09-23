//
// Created by Xomagat on 07.08.2026.
//

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "../../libs/BooleanValue.h"
#include "../../libs/NumberValue.h"
#include "../../libs/StringValue.h"
#include "../../libs/Value.h"
#include "Expression.h"

class ConditionalExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr1, expr2;
    std::string op;

public:
    explicit ConditionalExpression(std::string op,
        std::unique_ptr<Expression> expr1,
        std::unique_ptr<Expression> expr2) : op(op), expr1(std::move(expr1)), expr2(std::move(expr2))
    {

    }

    std::unique_ptr<Value> eval(Environment& env) const override
    {
        std::unique_ptr<Value> value1 = expr1->eval(env);
        std::unique_ptr<Value> value2 = expr2->eval(env);

        if (op == "&&")
        {
            auto v1 = expr1->eval(env);
            if (!v1->as_bool()) return std::make_unique<BooleanValue>(false);
            return std::make_unique<BooleanValue>(expr2->eval(env)->as_bool());
        }
        if (op == "||")
        {
            auto v1 = expr1->eval(env);
            if (v1->as_bool()) return std::make_unique<BooleanValue>(true);
            return std::make_unique<BooleanValue>(expr2->eval(env)->as_bool());
        }

        if (auto s1 = dynamic_cast<StringValue*>(value1.get()))
        {
            std::string s2 = value2->as_string();

            if (op == "==")
            {
                return std::make_unique<BooleanValue>(s1->as_string() == s2);
            }
            if (op == "!=")
            {
                return std::make_unique<BooleanValue>(s1->as_string() != s2);
            }

            throw std::runtime_error("Unknown operation!");
        }

        auto n1 = value1->as_number();
        auto n2 = value2->as_number();

        std::unordered_map<std::string, bool> operators = {
            {"==", n1 == n2},
            {"!=", n1 != n2},
            {"<=", n1 <= n2},
            {">=", n1 >= n2},
            {"<", n1 < n2},
            {">", n1 > n2},
        };

        auto it = operators.find(op);
        if (it == operators.end())
            throw std::runtime_error("Unknown operation!");
        return std::make_unique<BooleanValue>(it->second);
    }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* left = expr1->codegen(ctx);
        llvm::Value* right = expr2->codegen(ctx);

        bool isFloat = left->getType()->isDoubleTy() || right->getType()->isDoubleTy();

        if (isFloat)
        {
            left  = coerceToType(left,  ctx.builder.getDoubleTy(), ctx.builder);
            right = coerceToType(right, ctx.builder.getDoubleTy(), ctx.builder);

            if (op == "==") return ctx.builder.CreateFCmpOEQ(left, right);
            if (op == "!=") return ctx.builder.CreateFCmpONE(left, right);
            if (op == "<")  return ctx.builder.CreateFCmpOLT(left, right);
            if (op == ">")  return ctx.builder.CreateFCmpOGT(left, right);
            if (op == "<=") return ctx.builder.CreateFCmpOLE(left, right);
            if (op == ">=") return ctx.builder.CreateFCmpOGE(left, right);
            throw std::runtime_error("Codegen for this conditional operator not implemented yet!");
        }

        if (op == "==") return ctx.builder.CreateICmpEQ(left, right);
        if (op == "!=") return ctx.builder.CreateICmpNE(left, right);
        if (op == "<")  return ctx.builder.CreateICmpSLT(left, right);
        if (op == ">")  return ctx.builder.CreateICmpSGT(left, right);
        if (op == "<=") return ctx.builder.CreateICmpSLE(left, right);
        if (op == ">=") return ctx.builder.CreateICmpSGE(left, right);
        throw std::runtime_error("Codegen for this conditional operator not implemented yet!");
    }

    std::string to_str() const override
    {
        return expr1->to_str() + " " + op + " " + expr2->to_str();
    }
};