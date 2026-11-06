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

class FileScopeStatement : public Statement
{
    std::string file;
public:
    explicit FileScopeStatement(std::string f) : file(std::move(f)) {}
    void codegen(CodegenContext& ctx) const override { ctx.current_file = file; }
};

class NamespaceScopeStatement : public Statement
{
    std::string ns;
public:
    explicit NamespaceScopeStatement(std::string n) : ns(std::move(n)) {}
    void codegen(CodegenContext& ctx) const override { ctx.current_ns = ns; }
};

class UsingNamespaceStatement : public Statement
{
    std::string ns;
public:
    explicit UsingNamespaceStatement(std::string n) : ns(std::move(n)) {}
    void codegen(CodegenContext& ctx) const override { ctx.usings[ctx.current_file].push_back(ns); }
};