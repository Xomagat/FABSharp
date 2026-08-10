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

std::vector<std::unique_ptr<Statement>> Parser::parse()
{
    std::vector<std::unique_ptr<Statement>> result;

    while (!match(token_type::eof))
    {
        result.push_back(statement());
    }

    return result;
}

std::unique_ptr<Statement> Parser::statement()
{
    if (match(token_type::WRITE))
    {
        auto expr = expression();
        consume(token_type::SEMI);
        return std::make_unique<WriteStatement>(std::move(expr));
    }
    if (match(token_type::WRITELN))
    {
        auto expr = expression();
        consume(token_type::SEMI);
        return std::make_unique<WritelnStatement>(std::move(expr));
    }
    if (match(token_type::IF))
    {
        return if_else();
    }
    return assigment_statement();
}

std::unique_ptr<Statement> Parser::assigment_statement()
{
    // name = 33
    Token current = get(0);
    if (current.get_type() == token_type::WORD && get(1).get_type() == token_type::EQ)
    {
        consume(token_type::WORD);
        std::string var = current.get_text();
        consume(token_type::EQ);

        auto expr = expression();
        if (!match(token_type::SEMI))
            throw std::runtime_error("You miss the ;");

        return std::make_unique<AssigementStatement>(var, std::move(expr));
    }
    throw std::runtime_error("Unknown operator!");
}

std::unique_ptr<Statement> Parser::if_else()
{
    std::unique_ptr<Expression> condition = expression();
    consume(token_type::LBRACKET);
    get(1);
    std::unique_ptr<Statement> if_statement = statement();
    consume(token_type::RBRACKET);
    get(1);
    std::unique_ptr<Statement> else_statement = nullptr;

    if (match(token_type::ELSE))
    {
        consume(token_type::LBRACKET);
        get(1);
        else_statement = statement();
        consume(token_type::RBRACKET);
        get(1);
    }
    else
    {
        else_statement = nullptr;
    }

    return std::make_unique<IfStatement>(std::move(condition), std::move(if_statement), std::move(else_statement));
}

std::unique_ptr<Expression> Parser::expression()
{
    return conditional();
}

std::unique_ptr<Expression> Parser::conditional()
{

    std::unique_ptr<Expression> expr = additive();

    while (true)
    {
        if (match(token_type::CEQ))
        {
            expr = std::make_unique<ConditionalExpression>("==", std::move(expr), additive());
            continue;
        }
        if (match(token_type::LTEQ))
        {
            expr = std::make_unique<ConditionalExpression>(">=", std::move(expr), additive());
            continue;
        }
        if (match(token_type::GTEQ))
        {
            expr = std::make_unique<ConditionalExpression>("<=", std::move(expr), additive());
            continue;
        }
        if (match(token_type::LT))
        {
            expr = std::make_unique<ConditionalExpression>("<", std::move(expr), additive());
            continue;
        }
        if (match(token_type::GT))
        {
            expr = std::make_unique<ConditionalExpression>(">", std::move(expr), additive());
            continue;
        }
        break;
    }


    return expr;
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
    std::unique_ptr<Expression> expr = pow();

    while (true)
    {
        if (match(token_type::MULT))
        {
            expr = std::make_unique<BinExpression>('*', std::move(expr), pow());
            continue;
        }
        if (match(token_type::DIV))
        {
            expr = std::make_unique<BinExpression>('/', std::move(expr), pow());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::pow()
{
    std::unique_ptr<Expression> expr = unary();

    while (true)
    {
        if (match(token_type::POW))
        {
            expr = std::make_unique<BinExpression>('^', std::move(expr), unary());
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
        return std::make_unique<ValueExpression>(std::stod(current.get_text()));
    if (match(token_type::HEX_NUMBER))
        return std::make_unique<ValueExpression>(std::stol(current.get_text(), nullptr, 16));
    if (match(token_type::TEXT))
        return std::make_unique<ValueExpression>(current.get_text());
    if (match(token_type::WORD))
        return std::make_unique<VariableExpression>(current.get_text());
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

Token Parser::consume(token_type type)
{
    Token t = get(0);

    if (type != t.get_type())
        throw std::runtime_error("Token " + tokens_string[t.get_type()] + " does not match " + tokens_string[type]);

    pos++;
    return t;
}