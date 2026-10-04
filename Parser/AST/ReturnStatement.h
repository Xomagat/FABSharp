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
            context.builder.CreateRet(expr->codegen(context));
        else
            context.builder.CreateRetVoid();
    }
};