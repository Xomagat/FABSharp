//
// Created by Xomagat on 21.08.2026.
//

#pragma once
#include <string>
#include <vector>

#include "AssigementStatement.h"

#include "Expression.h"
#include "VariableExpression.h"
#include "../../libs/STDLibInfo.h"
#include "../../CodeGen/Runtimeatters.h"

#include "NamedCall.h"

#include <set>

inline const StdlibFunctionInfo* find_with_conversion(const std::string& name,
                                                      const std::vector<std::string>& actual)
{
    if (auto ex = stdlib_symbols.find(mangle_name(name, actual)); ex != stdlib_symbols.end())
        return &ex->second;

    const std::string prefix = name + "#" + std::to_string(actual.size()) + "_";

    const StdlibFunctionInfo* best = nullptr;
    int  best_cost = INT_MAX;
    bool tie = false;

    for (auto& [key, info] : stdlib_symbols)
    {
        if (!key.starts_with(prefix) || info.arg_types.size() != actual.size()) continue;

        int total = 0;
        bool ok = true;
        for (size_t i = 0; i < actual.size() && ok; ++i)
        {
            int c = conversion_cost(actual[i], info.arg_types[i]);
            if (c < 0) ok = false; else total += c;
        }
        if (!ok) continue;

        if (total < best_cost) { best = &info; best_cost = total; tie = false; }
        else if (total == best_cost) tie = true;
    }

    if (tie) throw std::runtime_error("Ambiguous call to '" + name + "'");
    return best;
}

inline const StdlibFunctionInfo* find_static_with_conversion(const std::string& cls, const std::string& name,
                                                             const std::vector<std::string>& actual)
{
    const std::string prefix = cls + "::" + name + "#" + std::to_string(actual.size()) + "_";

    const StdlibFunctionInfo* best = nullptr;
    int best_cost = INT_MAX;
    bool tie = false;

    for (auto& [key, info] : stdlib_statics)
    {
        if (!key.starts_with(prefix) || info.arg_types.size() != actual.size()) continue;

        int total = 0; bool ok = true;
        for (size_t i = 0; i < actual.size() && ok; ++i)
        {
            int c = conversion_cost(actual[i], info.arg_types[i]);
            if (c < 0) ok = false; else total += c;
        }
        if (!ok) continue;

        if (total < best_cost) { best = &info; best_cost = total; tie = false; }
        else if (total == best_cost) tie = true;
    }
    if (tie) throw std::runtime_error("Ambiguous call to '" + cls + "." + name + "'");
    return best;
}

inline llvm::Value* emit_spawn(CodegenContext& ctx, const std::string& fname,
                               const std::vector<llvm::Value*>& vals)
{
    auto& b = ctx.builder;

    std::vector<std::string> types;
    for (auto* v : vals) types.push_back(type_of(ctx, v));

    llvm::Function* fn = nullptr;
    std::string ret_name;
    int best = INT_MAX;
    for (auto& q : ns_candidates(ctx, fname))
    {
        if (auto it = ctx.functions.find(mangle_name(q, types)); it != ctx.functions.end())
        {
            fn = it->second;
            ret_name = ctx.func_ret[mangle_name(q, types)];
            break;
        }

        auto ov = ctx.overloads.find(q);
        if (ov == ctx.overloads.end()) continue;
        for (auto& sig : ov->second)
        {
            if (sig.types.size() != types.size()) continue;
            int total = 0; bool ok = true;
            for (size_t i = 0; i < types.size() && ok; ++i)
            {
                int c = conversion_cost(types[i], sig.types[i]);
                if (c < 0) ok = false; else total += c;
            }
            if (ok && total < best)
            {
                best = total;
                fn = ctx.functions.at(mangle_name(q, sig.types));
                ret_name = ctx.func_ret[mangle_name(q, sig.types)]; // NEW
            }
        }
        if (fn) break;
    }
    if (!fn)
        throw std::runtime_error("spawn: function '" + fname + "' with these arguments not found "
                                 "(it must be defined above; default arguments must be passed explicitly)");

    if (is_struct(ret_name))
        throw std::runtime_error("spawn: a function that returns a struct is not supported yet");

    auto* fty = fn->getFunctionType();
    std::vector<llvm::Type*> pt(fty->param_begin(), fty->param_end());

    std::vector<llvm::Type*> et{b.getPtrTy()};
    et.insert(et.end(), pt.begin(), pt.end());
    auto* envTy = llvm::StructType::get(ctx.context, et);

    auto newFn = ctx.module.getOrInsertFunction("fab_thread_new",
        llvm::FunctionType::get(b.getPtrTy(), {b.getPtrTy()}, false));
    llvm::Value* th = b.CreateCall(newFn, {b.CreateGlobalStringPtr(ret_name)});

    auto* nullp = llvm::ConstantPointerNull::get(b.getPtrTy());
    llvm::Value* size = b.CreatePtrToInt(b.CreateGEP(envTy, nullp, {b.getInt32(1)}), b.getInt64Ty());
    auto mallocFn = ctx.module.getOrInsertFunction("malloc",
        llvm::FunctionType::get(b.getPtrTy(), {b.getInt64Ty()}, false));
    llvm::Value* env = b.CreateCall(mallocFn, {size});

    b.CreateStore(th, b.CreateStructGEP(envTy, env, 0));
    for (size_t i = 0; i < vals.size(); ++i)
        b.CreateStore(coerceToType(vals[i], pt[i], b), b.CreateStructGEP(envTy, env, (unsigned)i + 1));

    auto* tty = llvm::FunctionType::get(b.getPtrTy(), {b.getPtrTy()}, false);
    auto* thunk = llvm::Function::Create(tty, llvm::Function::InternalLinkage, "spawn.thunk", ctx.module);
    thunk->addFnAttr(llvm::Attribute::NoUnwind);
    {
        llvm::IRBuilder<> tb(ctx.context);
        tb.SetInsertPoint(llvm::BasicBlock::Create(ctx.context, "entry", thunk));

        llvm::Value* e = thunk->getArg(0);
        llvm::Value* h = tb.CreateLoad(tb.getPtrTy(), tb.CreateStructGEP(envTy, e, 0));

        std::vector<llvm::Value*> a;
        for (size_t i = 0; i < pt.size(); ++i)
            a.push_back(tb.CreateLoad(pt[i], tb.CreateStructGEP(envTy, e, (unsigned)i + 1)));

        llvm::Value* r = tb.CreateCall(fn, a);

        if (!fty->getReturnType()->isVoidTy())
        {
            auto resFn = ctx.module.getOrInsertFunction("fab_thread_result",
                llvm::FunctionType::get(tb.getPtrTy(), {tb.getPtrTy()}, false));
            tb.CreateStore(r, tb.CreateCall(resFn, {h}));
        }

        tb.CreateCall(ctx.module.getOrInsertFunction("free",
            llvm::FunctionType::get(tb.getVoidTy(), {tb.getPtrTy()}, false)), {e});

        tb.CreateCall(ctx.module.getOrInsertFunction("fab_thread_finish",
            llvm::FunctionType::get(tb.getVoidTy(), {tb.getPtrTy()}, false)), {h});

        tb.CreateRet(llvm::ConstantPointerNull::get(tb.getPtrTy()));
    }

    auto startFn = ctx.module.getOrInsertFunction("fab_thread_start",
        llvm::FunctionType::get(b.getPtrTy(), {b.getPtrTy(), b.getPtrTy(), b.getPtrTy()}, false));
    llvm::Value* r = b.CreateCall(startFn, {th, thunk, env});
    ctx.value_types[r] = "thread";
    return r;
}

inline llvm::Value* call_user_converted(CodegenContext& ctx, const std::string& q,
                                        const std::vector<llvm::Value*>& vals,
                                        const std::vector<std::string>& types)
{
    auto ov = ctx.overloads.find(q);
    if (ov == ctx.overloads.end()) return nullptr;

    const FuncSig* best = nullptr;
    int  best_cost = INT_MAX;
    bool tie = false;

    for (auto& sig : ov->second)
    {
        if (sig.types.size() < types.size()) continue;

        bool ok = true;

        for (size_t i = types.size(); i < sig.types.size() && ok; ++i)
            if (i >= sig.defaults.size() || !sig.defaults[i]) ok = false;
        if (!ok) continue;

        int total = 0;
        for (size_t i = 0; i < types.size() && ok; ++i)
        {
            int c = conversion_cost(types[i], sig.types[i]);
            if (c < 0) ok = false; else total += c;
        }
        if (!ok) continue;

        if (total < best_cost)       { best = &sig; best_cost = total; tie = false; }
        else if (total == best_cost) tie = true;
    }

    if (!best) return nullptr;
    if (tie) throw std::runtime_error("Ambiguous call to '" + q + "'");

    auto key = mangle_name(q, best->types);
    llvm::Function* fn = ctx.functions.at(key);
    auto* fty = fn->getFunctionType();

    std::vector<llvm::Value*> out = vals;
    for (size_t i = out.size(); i < best->types.size(); ++i)
        out.push_back(best->defaults[i]->codegen_expected(ctx, best->types[i]));

    for (size_t i = 0; i < out.size(); ++i)
        out[i] = coerceToType(out[i], fty->getParamType(i), ctx.builder);

    llvm::Value* r = ctx.builder.CreateCall(fn, out);
    auto& rt = ctx.func_ret[key];
    if (rt != "void") ctx.value_types[r] = rt;
    return r;
}

class FunctionalExpression : public Expression
{
private:
    std::string name;
    std::vector<std::unique_ptr<Expression>> args;

    std::vector<std::string> names;

public:
    void add_arg(std::string n, std::unique_ptr<Expression> a)
    { names.resize(args.size()); names.push_back(std::move(n)); args.push_back(std::move(a)); }
    bool has_named() const { return any_named(names); }

    explicit FunctionalExpression(std::string name) : name(name) {}

    explicit FunctionalExpression(std::string name, std::vector<std::unique_ptr<Expression>> args) : name(name), args(std::move(args)) {}

    void add_arg(std::unique_ptr<Expression> arg)
    {
        args.push_back(std::move(arg));
    }

    static llvm::Value* emit_stdlib_call(CodegenContext& ctx, const StdlibFunctionInfo& info,
                                     const std::vector<llvm::Value*>& given)
    {
        std::vector<llvm::Value*> vals = given;
        for (size_t i = vals.size(); i < info.arg_types.size(); ++i)
        {
            if (i >= info.defaults.size() || info.defaults[i].empty())
                throw std::runtime_error("Missing argument for " + info.symbol);
            vals.push_back(emit_stdlib_const(ctx, {"", info.arg_types[i], info.defaults[i], true}));
        }

        std::vector<llvm::Type*> params;
        for (auto& t : info.arg_types)
            params.push_back(type_to_llvm(t, ctx.builder));

        if (params.size() != vals.size())
            throw std::runtime_error("Internal error: argument count mismatch for " + info.symbol);

        auto* fnType = llvm::FunctionType::get(type_to_llvm(info.return_type, ctx.builder), params, false);
        auto callee  = ctx.module.getOrInsertFunction(info.symbol, fnType);
        if (auto* f = llvm::dyn_cast<llvm::Function>(callee.getCallee())) rt_apply_attrs(f);

        std::vector<llvm::Value*> coerced;
        for (size_t i = 0; i < vals.size(); ++i)
            coerced.push_back(coerceToType(vals[i], params[i], ctx.builder));

        if (llvm::Value* r = rt_try_intrinsic(ctx.builder, info.symbol, coerced))
        {
            ctx.value_types[r] = info.return_type;
            return r;
        }

        llvm::Value* r = ctx.builder.CreateCall(callee, coerced);
        ctx.value_types[r] = info.return_type;
        return r;
    }

    static llvm::Value* call(CodegenContext& context, const std::string& full_name,
                         std::vector<llvm::Value*> args_values)
    {
        std::string name = full_name;
        bool std_only = name.starts_with("std::");
        if (std_only) name = name.substr(5);

        std::vector<std::string> arg_type_names;
        for (auto* v : args_values)
            arg_type_names.push_back(type_of(context, v));

        std::vector<std::string> cands = std_only ? std::vector<std::string>{name}
                                                  : ns_candidates(context, name);

        if (!std_only)
            for (auto& q : cands)
                if (auto* r = call_user_converted(context, q, args_values, arg_type_names))
                    return r;

        std::vector<std::string> widened = arg_type_names;
        bool changed = false;
        for (auto& t : widened)
            if (t == "int" || t == "short" || t == "long" || t == "float")
            { t = "double"; changed = true; }

        if (changed)
        {
            auto wIt = stdlib_symbols.find(mangle_name(name, widened));
            if (wIt != stdlib_symbols.end())
                return emit_stdlib_call(context, wIt->second, args_values);
        }

        std::string sig = full_name + "(";
        for (size_t i = 0; i < arg_type_names.size(); ++i)
            sig += (i ? ", " : "") + arg_type_names[i];
        sig += ")";

        if (auto* info = find_with_conversion(name, arg_type_names))
            return emit_stdlib_call(context, *info, args_values);

        throw std::runtime_error("Unknown function or overload for codegen: " + sig);
    }

    llvm::Value* try_struct_ctor(CodegenContext& ctx) const
    {
        std::string st;
        for (auto& q : ns_candidates(ctx, name))
            if (is_struct(q)) { st = q; break; }
        if (st.empty()) return nullptr;

        auto& b = ctx.builder;
        auto& ci = get_class(ctx, st);
        if (args.size() > ci.fields.size())
            throw std::runtime_error("Too many arguments for struct '" + st + "'");

        llvm::Value* v = emit_struct_default(ctx, st);
        for (size_t i = 0; i < args.size(); ++i)
        {
            size_t idx = i;
            if (i < names.size() && !names[i].empty())
            {
                int k = ci.field_index(names[i]);
                if (k < 0) throw std::runtime_error("Struct '" + st + "' has no field '" + names[i] + "'");
                idx = (size_t)k;
            }
            auto& f = ci.fields[idx];
            llvm::Type* lt = type_to_llvm(f.type, b);
            llvm::Value* a = args[i]->is_null_literal()
                ? llvm::Constant::getNullValue(lt)
                : args[i]->codegen_expected(ctx, f.type);
            v = b.CreateInsertValue(v, coerceToType(a, lt, b), {(unsigned)idx});
        }
        ctx.value_types[v] = st;
        return v;
    }

    llvm::Value* codegen_named(CodegenContext& ctx) const
    {
        std::string base = name;
        bool std_only = base.starts_with("std::");
        if (std_only) base = base.substr(5);

        for (auto& q : std_only ? std::vector<std::string>{base} : ns_candidates(ctx, base))
        {
            if (!std_only)
                if (auto ov = ctx.overloads.find(q); ov != ctx.overloads.end())
                {
                    std::vector<NamedCand> user;
                    for (auto& sig : ov->second)
                    {
                        NamedCand c;
                        c.types = sig.types;  c.names = sig.names;
                        for (size_t p = 0; p < sig.types.size(); ++p)
                            c.has_default.push_back(p < sig.defaults.size() && sig.defaults[p]);

                        auto key = mangle_name(q, sig.types);
                        llvm::Function* fn = ctx.functions.at(key);
                        const FuncSig* sp = &sig;
                        c.make_default = [sp](CodegenContext& cx, size_t p) {
                            return sp->defaults[p]->codegen_expected(cx, sp->types[p]);
                        };
                        c.emit = [fn, key](CodegenContext& cx, std::vector<llvm::Value*>& v) -> llvm::Value* {
                            llvm::Value* r = cx.builder.CreateCall(fn, v);
                            auto& rt = cx.func_ret[key];
                            if (rt != "void") cx.value_types[r] = rt;
                            return r;
                        };
                        user.push_back(std::move(c));
                    }
                    if (auto* r = emit_named_call(ctx, args, names, user, name, true)) return r;
                }

            std::vector<NamedCand> lib;
            std::set<std::string> seen;
            for (auto& [k, inf] : stdlib_symbols)
                if (k.starts_with(q + "#") && seen.insert(inf.symbol).second)
                    lib.push_back(lib_cand(inf, 0, nullptr));
            if (auto* r = emit_named_call(ctx, args, names, lib, name, true)) return r;
        }
        throw std::runtime_error("Unknown function or overload for named arguments: " + name);
    }

    static NamedCand lib_cand(const StdlibFunctionInfo& info, size_t skip, llvm::Value* self)
    {
        NamedCand c;
        for (size_t i = skip; i < info.arg_types.size(); ++i)
        {
            c.types.push_back(info.arg_types[i]);
            c.names.push_back(i < info.names.size() ? info.names[i] : "");
            c.has_default.push_back(i < info.defaults.size() && !info.defaults[i].empty());
        }
        c.make_default = [info, skip](CodegenContext& ctx, size_t p) {
            size_t i = p + skip;
            return emit_stdlib_const(ctx, {"", info.arg_types[i], info.defaults[i], true});
        };
        c.emit = [info, self](CodegenContext& ctx, std::vector<llvm::Value*>& v) {
            std::vector<llvm::Value*> all;
            if (self) all.push_back(self);
            all.insert(all.end(), v.begin(), v.end());
            return emit_stdlib_call(ctx, info, all);
        };
        return c;
    }

    llvm::Value* codegen(CodegenContext& context) const override
    {
        if (auto* s = try_struct_ctor(context)) return s;

        if (name == "spawn" && !args.empty() && !has_named()
        && !context.overloads.contains("spawn"))
        {
            auto* fv = dynamic_cast<VariableExpression*>(args[0].get());
            if (!fv)
                throw std::runtime_error("spawn: first argument must be a function name");

            std::vector<llvm::Value*> vals;
            for (size_t i = 1; i < args.size(); ++i)
                vals.push_back(args[i]->codegen(context));
            return emit_spawn(context, fv->to_str(), vals);
        }

        if (has_named()) return codegen_named(context);

        std::vector<llvm::Value*> vals;
        for (auto& a : args) vals.push_back(a->codegen(context));
        return call(context, name, std::move(vals));
    }

    std::string to_str() const override
    {
        return "define " + name + "(args count: " + std::to_string(args.size()) + ")";
    }
};
