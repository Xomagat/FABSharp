//
// Created by Xomagat on 07.08.2026.
//

#ifndef FABSHARP_PARSER_H
#define FABSHARP_PARSER_H

#include <string>
#include <vector>
#include <memory>

#include "AST/BinExpression.h"
#include "AST/ConditionalExpression.h"
#include "AST/Expression.h"
#include "AST/UnaryExpression.h"
#include "AST/ValueExpression.h"
#include "AST/VariableExpression.h"
#include "AST/FunctionalExpression.h"

#include "AST/Statement.h"
#include "AST/IfStatement.h"
#include "AST/LoopStatement.h"
#include "AST/BreakStatement.h"
#include "AST/ContinueStatement.h"
#include "AST/FunctionStatement.h"
#include "AST/AssigementStatement.h"

#include "AST/IOStatement.h"

#include "Token.h"
#include "TokenType.h"

class Parser
{
private:
    Token eof;
    std::vector<Token> tokens;

    int pos;
    int size;

    Token get(int relative_position);

    std::unique_ptr<Statement> statement();
    std::unique_ptr<Statement> assigment_statement(bool no_semi = false);
    std::unique_ptr<Statement> if_else();
    std::unique_ptr<Statement> while_statement();
    std::unique_ptr<Statement> do_while_statement();
    std::unique_ptr<Statement> for_statement();
    std::unique_ptr<Statement> statement_or_block();
    std::unique_ptr<Statement> block();

    std::unique_ptr<FunctionDefineStatement> define_function();

    std::unique_ptr<Expression> function();
    std::unique_ptr<Expression> expression();
    std::unique_ptr<Expression> conditional();
    std::unique_ptr<Expression> additive();
    std::unique_ptr<Expression> multiply();
    std::unique_ptr<Expression> pow();
    std::unique_ptr<Expression> unary();
    std::unique_ptr<Expression> primary();
    std::unique_ptr<Expression> equality();
    std::unique_ptr<Expression> logic_or();
    std::unique_ptr<Expression> logic_and();

    bool match(token_type type);

    Token consume(token_type type);

public:
    Parser(std::vector<Token> tokens);

    std::vector<std::unique_ptr<Statement>> parse();
};

#endif // FABSHARP_PARSER_H
