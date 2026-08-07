//
// Created by Xomagat on 07.08.2026.
//

#include "Parser.h"

// vars

// funcs
Parser::Parser(std::vector<Token> tokens)
{
    eof = Token(token_type::eof, "");

    this->tokens = tokens;

    size = tokens.size();

    pos = 0;
}

std::vector<std::unique_ptr<Expression>> Parser::parse()
{
    std::vector<std::unique_ptr<Expression>> result;

    while (!match(token_type::eof))
    {
        result.push_back(expression());
    }

    return result;
}

std::unique_ptr<Expression> Parser::expression()
{
    return additive();
}

std::unique_ptr<Expression> Parser::additive()
{

    std::unique_ptr<Expression> expr = multiply();

    while (true)
    {
        if (match(token_type::PLUS))
        {
            expr = std::make_unique<BinExpression>('+', std::move(expr), multiply());
            continue;
        }
        if (match(token_type::MINUS))
        {
            expr = std::make_unique<BinExpression>('-', std::move(expr), multiply());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::multiply()
{
    std::unique_ptr<Expression> expr = unary();

    while (true)
    {
        if (match(token_type::MULT))
        {
            expr = std::make_unique<BinExpression>('*', std::move(expr), unary());
            continue;
        }
        if (match(token_type::DIV))
        {
            expr = std::make_unique<BinExpression>('/', std::move(expr), unary());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::unary()
{
    if (match(token_type::MINUS))
        return std::make_unique<UnaryExpression>('-', std::move(primary()));
    if (match(token_type::PLUS))
        return primary();

    return primary();
}

std::unique_ptr<Expression> Parser::primary()
{
    Token current = get(0);

    if (match(token_type::NUMBER))
        return std::make_unique<NumberExpression>(std::stod(current.get_text()));
    if (match(token_type::HEX_NUMBER))
        return std::make_unique<NumberExpression>(std::stol(current.get_text(), nullptr, 16));
    if (match(token_type::LPARENT))
    {
        std::unique_ptr<Expression> result = expression();
        match(token_type::RPARENT);
        return result;
    }

    throw std::runtime_error("Unknown expression!");
}

Token Parser::get(int relative_position)
{
    int position = pos + relative_position;
    if (position >= size)
        return eof;

    return tokens[position];
}

bool Parser::match(token_type type)
{
    Token t = get(0);

    if (type != t.get_type())
        return false;

    pos++;
    return true;
}