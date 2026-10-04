//
// Created by Xomagat on 26.09.2026.
//

#pragma once
#include "Statement.h"

#include "../../CodeGen/CodegenContext.h"

class UseStatement : public Statement
{
private:
    std::string name;

public:
    explicit UseStatement(std::string& name) : name(name) {}

    void codegen(CodegenContext &context) const override { /*
        Symbols are registered in `stdlib_symbols` during the parsing stage,
        while the `UseStatement` in the AST remains merely a "marker."*/ }
};