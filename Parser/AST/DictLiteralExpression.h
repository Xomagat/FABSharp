//
// Created by Xomagat on 07.10.2026.
//

#pragma once
#include <memory>
#include <vector>

#include "Expression.h"
#include "FunctionalExpression.h"

class DictLiteralExpression : public Expression
{
private:
    using Pair = std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>;
    std::vector<Pair> pairs;

public:
    explicit DictLiteralExpression() {}

    void add_pair(std::unique_ptr<Expression> key, std::unique_ptr<Expression> val)
    {
        pairs.emplace_back(std::move(key), std::move(val));
    }

    llvm::Value* codegen_expected(CodegenContext& ctx, const std::string& expected) const override
    {
        if (!expected.starts_with("dict<"))
            throw std::runtime_error("Dict literal can only initialize a dict<K,V>!");

        auto [k, v] = dict_kv(expected);
        llvm::Value* d = emit_container_new(ctx, expected);

        StdlibFunctionInfo set_info{"fab_dict_set_" + k + "_" + v, "void", {expected, k, v}};

        for (auto& [key, val] : pairs)
        {
            llvm::Value* kv = key->codegen_expected(ctx, k);
            llvm::Value* vv = val->is_null_literal()
                ? llvm::Constant::getNullValue(type_to_llvm(v, ctx.builder))
                : val->codegen_expected(ctx, v);

            FunctionalExpression::emit_stdlib_call(ctx, set_info, {d, kv, vv});
        }
        return d;
    }

    llvm::Value* codegen(CodegenContext&) const override
    {
        throw std::runtime_error("Dict literal needs a type context: dict<K,V> x = {...};");
    }

    std::string to_str() const override
    {
        return "";
    }
};