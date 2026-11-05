#pragma once
#include <memory>
#include <string>

#include "Expression.h"
#include "AssigementStatement.h"
#include "VariableExpression.h"
#include "IndexExprAndStat.h"
#include "ClassStatement.h"

class IncDecExpression : public Expression
{
private:
    std::unique_ptr<Expression> expr;
    bool is_inc;
    bool is_pref;

    struct LValue { llvm::Value* ptr = nullptr; llvm::Type* ty = nullptr; std::string type; };

    LValue resolve(CodegenContext& ctx) const
    {
        auto& b = ctx.builder;
        LValue lv;

        if (auto* var = dynamic_cast<VariableExpression*>(expr.get()))
        {
            std::string name = var->to_str();
            if (ctx.const_vars.contains(name))
                throw std::runtime_error("Cannot modify const variable '" + name + "'!");

            auto it = ctx.variables.find(name);
            if (it != ctx.variables.end())
            {
                lv.ptr  = it->second;
                lv.ty   = it->second->getAllocatedType();
                lv.type = ctx.var_types[name];
            }
            else if (auto* p = emit_this_field_ptr(ctx, name, lv.type))
            {
                lv.ptr = p;
                lv.ty  = type_to_llvm(lv.type, b);
            }
            else if (auto* g = find_global(ctx, name))
            {
                if (g->is_const)
                    throw std::runtime_error("Cannot modify const variable '" + name + "'!");
                lv.ptr  = g->gv;
                lv.ty   = g->gv->getValueType();
                lv.type = g->type;
            }
            else
                throw std::runtime_error("Variable {" + name + "} not found!");
        }
        else if (auto* ie = dynamic_cast<IndexExpression*>(expr.get()))
        {
            lv.ptr = ie->codegen_ptr(ctx, lv.type);       // arr[i]++
            lv.ty  = type_to_llvm(lv.type, b);
        }
        else if (auto* fe = dynamic_cast<FieldExpression*>(expr.get()))
        {
            lv.ptr = fe->codegen_ptr(ctx, lv.type);       // obj.x++
            lv.ty  = type_to_llvm(lv.type, b);
        }
        else
            throw std::runtime_error("Increment/Decrement requires a variable, list element or field!");

        return lv;
    }

public:
    IncDecExpression(std::unique_ptr<Expression> expr, bool is_inc, bool is_pref)
        : expr(std::move(expr)), is_inc(is_inc), is_pref(is_pref) {}

    llvm::Value* codegen(CodegenContext& ctx) const override
    {
        auto& b = ctx.builder;
        LValue lv = resolve(ctx);

        llvm::Value* oldValue = b.CreateLoad(lv.ty, lv.ptr, "old_val");
        llvm::Value* newValue = nullptr;

        if (lv.ty->isIntegerTy() && !lv.ty->isIntegerTy(1))
        {
            llvm::Value* one = llvm::ConstantInt::get(lv.ty, 1);
            newValue = is_inc ? b.CreateNSWAdd(oldValue, one, "inc")
                              : b.CreateNSWSub(oldValue, one, "dec");
        }
        else if (lv.ty->isFloatingPointTy())
        {
            llvm::Value* one = llvm::ConstantFP::get(lv.ty, 1.0);
            newValue = is_inc ? b.CreateFAdd(oldValue, one, "inc")
                              : b.CreateFSub(oldValue, one, "dec");
        }
        else
            throw std::runtime_error("Unsupported type '" + lv.type + "' for ++/--");

        b.CreateStore(newValue, lv.ptr);

        llvm::Value* result = is_pref ? newValue : oldValue;
        ctx.value_types[result] = lv.type;
        return result;
    }

    std::string to_str() const override
    {
        std::string op = is_inc ? "++" : "--";
        return is_pref ? (op + expr->to_str()) : (expr->to_str() + op);
    }
};