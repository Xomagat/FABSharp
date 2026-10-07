//
// Created by Xomagat on 07.10.2026.
//

#pragma once
#include <memory>
#include <vector>

#include "Expression.h"
#include "FunctionalExpression.h"

class ListLiteralExpression : public Expression
{
private:
    std::vector<std::unique_ptr<Expression>> items;

public:
    explicit ListLiteralExpression() {}

    void add_item(std::unique_ptr<Expression> item) { items.push_back(std::move(item)); }

    llvm::Value *codegen(CodegenContext &context) const override
    {
        return llvm::Constant::getNullValue(0);
    }

    llvm::Value* codegen_expected(CodegenContext& ctx, const std::string& expected) const override
    {
        if (!expected.starts_with("list<"))
            throw std::runtime_error("List literal can only initialize a list<T>!");

        std::string elem = list_elem(expected);
        llvm::Value* list = emit_list_new(ctx, expected);

        for (auto& item : items)
        {
            StdlibFunctionInfo info{"fab_list_addend_" + elem, "void", {expected, elem}};
            FunctionalExpression::emit_stdlib_call(ctx, info, {list, item->codegen(ctx)});
        }
        return list;
    }

    std::string to_str() const override
    {
        return "";
    }
};