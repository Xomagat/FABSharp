//
// Created by Xomagat on 25.08.2026.
//

#pragma once
#include <memory>

#include "Expression.h"
#include "Statement.h"

class ReturnStatement : public Statement
{
private:
    std::unique_ptr<Expression> expr;

public:
    explicit ReturnStatement(std::unique_ptr<Expression> expr) : expr(std::move(expr)) {}

    void codegen(CodegenContext &context) const override
    {
        if (expr)
        {
            auto* it = type_to_llvm(context.current_ret, context.builder);
            llvm::Value* v = expr->is_null_literal() ? llvm::Constant::getNullValue(it)
                                             : expr->codegen_expected(context, context.current_ret);
            context.builder.CreateRet(coerceToType(v, it, context.builder));
        }
        else
            context.builder.CreateRetVoid();
    }
};