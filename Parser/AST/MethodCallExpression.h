//
// Created by Xomagat on 05.10.2026.
//

#include "Expression.h"

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Expression.h"
#include "FunctionalExpression.h"

class MethodCallExpression : public Expression
{
private:
    std::string name;
    std::unique_ptr<Expression> receiver;
    std::vector<std::unique_ptr<Expression>> args;

public:
    explicit MethodCallExpression(std::string& name, std::unique_ptr<Expression> receiver) : name(name), receiver(std::move(receiver)) {}

    void add_arg(std::unique_ptr<Expression> arg) { args.push_back(std::move(arg)); }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* self = receiver->codegen(ctx);
        std::string recv_type = type_of(ctx, self);

        std::vector<llvm::Value*> vals{self};
        std::vector<std::string> tail_types;
        for (auto& a : args)
        {
            vals.push_back(a->codegen(ctx));
            tail_types.push_back(type_of(ctx, vals.back()));
        }

        if (is_class(recv_type))
        {
            auto& ci = get_class(ctx, recv_type);
            auto m = ci.methods.find(mangle_name(name, tail_types));
            if (m == ci.methods.end())
                throw std::runtime_error("Class '" + recv_type + "' has no method '" + name + "' with these arguments");
            if (!m->second.is_public && ctx.current_class != recv_type)
                throw std::runtime_error("Method '" + name + "' is private!");

            std::vector<llvm::Value*> cargs{self};
            for (size_t i = 0; i < m->second.args.size(); ++i)
                cargs.push_back(coerceToType(vals[i + 1], type_to_llvm(m->second.args[i], ctx.builder), ctx.builder));

            emit_null_check(ctx, self, "Null reference: method '" + name + "'");

            llvm::Value* r = ctx.builder.CreateCall(m->second.fn, cargs);
            if (m->second.ret != "void") ctx.value_types[r] = m->second.ret;
            return r;
        }

        auto it = stdlib_methods.find(mangle_method(recv_type, name, tail_types));
        if (recv_type.starts_with("list<") || recv_type.starts_with("dict<"))
        {
            auto find_generic = [&](const char* kind) {
                std::string prefix = std::string(kind) + "::" + name + "#" + std::to_string(tail_types.size());
                for (auto i = stdlib_methods.begin(); i != stdlib_methods.end(); ++i)
                    if (i->first.starts_with(prefix) &&
                        (i->first.size() == prefix.size() || i->first[prefix.size()] == '_'))
                        return i;
                return stdlib_methods.end();
            };

            bool is_list = recv_type.starts_with("list<");
            auto git = find_generic(is_list ? "list" : "dict");
            if (git == stdlib_methods.end())
                throw std::runtime_error("Type '" + recv_type + "' has no method '" + name + "'");

            std::unordered_map<std::string, std::string> subst;
            std::string suffix;
            if (is_list) { subst = {{"T", list_elem(recv_type)}}; suffix = "_" + list_elem(recv_type); }
            else         { auto [k, v] = dict_kv(recv_type);
                subst = {{"K", k}, {"V", v}};        suffix = "_" + k + "_" + v; }

            auto fix = [&](std::string& t) {
                if (t == "list<T>" || t == "dict<K,V>") t = recv_type;
                else if (auto s = subst.find(t); s != subst.end()) t = s->second;
            };

            StdlibFunctionInfo info = git->second;
            info.symbol += suffix;
            for (auto& t : info.arg_types) fix(t);
            fix(info.return_type);

            return FunctionalExpression::emit_stdlib_call(ctx, info, vals);
        }

        if (it == stdlib_methods.end())
            throw std::runtime_error("Type '" + recv_type + "' has no method '" + name + "' with these arguments");

        return FunctionalExpression::emit_stdlib_call(ctx, it->second, vals);
    }

    std::string to_str() const override
    {
        return receiver->to_str() + "." + name + "(...)";
    }
};
