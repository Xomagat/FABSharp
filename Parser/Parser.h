//
// Created by Xomagat on 07.08.2026.
//

#ifndef FABSHARP_PARSER_H
#define FABSHARP_PARSER_H

#include <string>
#include <vector>
#include <memory>

#include "AST/UnaryExpression.h"

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

    std::unique_ptr<Expression> expression();
    std::unique_ptr<Expression> additive();
    std::unique_ptr<Expression> multiply();
    std::unique_ptr<Expression> unary();
    std::unique_ptr<Expression> primary();

    bool match(token_type type);

public:
    Parser(std::vector<Token> tokens);

    std::vector<std::unique_ptr<Expression>> parse();
};

#endif // FABSHARP_PARSER_H
