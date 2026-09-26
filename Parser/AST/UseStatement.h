//
// Created by Xomagat on 26.09.2026.
//

#pragma once
#include "Statement.h"

#include "../../libs/Environment.h"

#include "../../CodeGen/CodegenContext.h"

class UseStatement : public Statement
{
private:
    std::string name;

public:
    explicit UseStatement(std::string& name) : name(name) {}

    void execute(Environment &env) const override {}
    void codegen(CodegenContext &context) const override {}
};