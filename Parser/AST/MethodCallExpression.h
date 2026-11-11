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
    std::vector<std::string> names;

    llvm::Value* emit_virtual(CodegenContext& ctx, llvm::Value* self, const MethodInfo& mi,
                              const std::vector<llvm::Value*>& cargs) const
    {
        auto& b = ctx.builder;
        if (receiver->to_str() != "this")
            emit_null_check(ctx, self, "Null reference: method '" + name + "'");

        llvm::Value* vptr = b.CreateLoad(b.getPtrTy(), self);
        llvm::Value* fp   = b.CreateLoad(b.getPtrTy(),
                                b.CreateConstGEP1_32(b.getPtrTy(), vptr, mi.slot));
        llvm::Value* r = b.CreateCall(mi.fn->getFunctionType(), fp, cargs);
        if (mi.ret != "void") ctx.value_types[r] = mi.ret;
        return r;
    }

    bool has_arity(const ClassInfo& ci) const
    {
        std::string pre = name + "#" + std::to_string(args.size());
        for (auto& [k, v] : ci.methods)
            if (k.starts_with(pre) && (k.size() == pre.size() || k[pre.size()] == '_'))
                return true;
        return false;
    }

    llvm::Value* codegen_named(CodegenContext& ctx, llvm::Value* self,
                               const std::string& recv_type) const
    {
        std::vector<NamedCand> cands;
        std::string what = recv_type + "." + name;

        if (is_class(recv_type))
        {
            auto& ci = get_class(ctx, recv_type);
            for (auto& [key, mi] : ci.methods)
            {
                if (!key.starts_with(name + "#")) continue;

                NamedCand c = sig_cand(mi);
                const MethodInfo* mp = &mi;
                c.emit = [this, self, mp, recv_type](CodegenContext& cx,
                                                     std::vector<llvm::Value*>& v) -> llvm::Value*
                {
                    if (!mp->is_public && cx.current_class != recv_type)
                        throw std::runtime_error("Method '" + name + "' is private!");
                    std::vector<llvm::Value*> cargs{self};
                    cargs.insert(cargs.end(), v.begin(), v.end());
                    return emit_virtual(cx, self, *mp, cargs);
                };
                cands.push_back(std::move(c));
            }
            return emit_named_call(ctx, args, names, cands, what);
        }

        std::set<std::string> seen;
        auto add = [&](const StdlibFunctionInfo& inf) {
            if (seen.insert(inf.symbol).second)
                cands.push_back(FunctionalExpression::lib_cand(inf, 1, self));
        };

        if (recv_type.starts_with("list<") || recv_type.starts_with("dict<"))
        {
            bool is_list = recv_type.starts_with("list<");
            std::unordered_map<std::string, std::string> subst;
            std::string suffix;
            if (is_list) { subst = {{"T", list_elem(recv_type)}}; suffix = "_" + list_elem(recv_type); }
            else { auto [k, v] = dict_kv(recv_type); subst = {{"K", k}, {"V", v}}; suffix = "_" + k + "_" + v; }

            auto fix = [&](std::string& t) {
                if (t == "list<T>" || t == "dict<K,V>") t = recv_type;
                else if (auto s = subst.find(t); s != subst.end()) t = s->second;
            };

            std::string pre = std::string(is_list ? "list" : "dict") + "::" + name + "#";
            for (auto& [key, inf] : stdlib_methods)
            {
                if (!key.starts_with(pre)) continue;
                StdlibFunctionInfo info = inf;
                info.symbol += suffix;
                for (auto& t : info.arg_types) fix(t);
                fix(info.return_type);
                add(info);
            }
            emit_null_check(ctx, self, "Null reference: method '" + name + "'");
        }
        else
        {
            for (std::string t = recv_type; !t.empty(); )
            {
                std::string pre = t + "::" + name + "#";
                for (auto& [key, inf] : stdlib_methods)
                    if (key.starts_with(pre)) add(inf);

                auto lt = lib_types.find(t);
                if (lt == lib_types.end()) break;
                t = lt->second.parent;
            }
        }
        return emit_named_call(ctx, args, names, cands, what);
    }

public:
    explicit MethodCallExpression(std::string& name, std::unique_ptr<Expression> receiver)
        : name(name), receiver(std::move(receiver)) {}

    void add_arg(std::string n, std::unique_ptr<Expression> a)
    {
        names.resize(args.size());
        names.push_back(std::move(n));
        args.push_back(std::move(a));
    }
    void add_arg(std::unique_ptr<Expression> a) { add_arg("", std::move(a)); }
    bool has_named() const { return any_named(names); }

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        llvm::Value* self = receiver->codegen(ctx);
        std::string recv_type = type_of(ctx, self);

        if (has_named() || (is_class(recv_type) && !has_arity(get_class(ctx, recv_type))))
            return codegen_named(ctx, self, recv_type);

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
                cargs.push_back(coerceToType(vals[i + 1],
                                type_to_llvm(m->second.args[i], ctx.builder), ctx.builder));
            return emit_virtual(ctx, self, m->second, cargs);
        }

        const StdlibFunctionInfo* it = find_lib_method(recv_type, name, tail_types);
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

            emit_null_check(ctx, self, "Null reference: method '" + name + "'");

            return FunctionalExpression::emit_stdlib_call(ctx, info, vals);
        }

        if (!it)
            throw std::runtime_error("Type '" + recv_type + "' has no method '" + name + "' with these arguments");

        return FunctionalExpression::emit_stdlib_call(ctx, *it, vals);
    }

    std::string to_str() const override
    {
        return receiver->to_str() + "." + name + "(...)";
    }
};
