//
// Created by Xomagat on 06.08.2026.
//

#include "Lexer.h"

// vars

// funcs
Lexer::Lexer(std::string code)
{
    OPERATION_CHARS = "+-*/";
    OPERATION_TYPE = {
        token_type::PLUS,
        token_type::MINUS,
        token_type::MULT,
        token_type::DIV,
    };

    this->code = code;

    length = code.length();
    pos = 0;

    tokens = std::vector<Token>();
}

std::vector<Token> Lexer::tokenize()
{
    while (pos < length)
    {
        char current = peek(0);

        if (isdigit(current))
        {
            tokenizeNumber();
        }
        else if (OPERATION_CHARS.find(current) != std::string::npos)
        {
            tokenizeOperation();
        }
        else
        {
            next(); // whitespaces
        }
    }

    return tokens;
}

void Lexer::tokenizeNumber()
{
    std::string buffer;
    char current = peek(0);

    while (isdigit(current))
    {
        buffer.push_back(current);
        current = next();
    }

    add_token(token_type::NUMBER, buffer);
}

void Lexer::tokenizeOperation()
{
    int position = OPERATION_CHARS.find(peek(0));
    add_token(OPERATION_TYPE[position]);
    next();
}

void Lexer::add_token(token_type type)
{
    add_token(type, "");
}

void Lexer::add_token(token_type type, std::string text)
{
    tokens.push_back(Token(type, text));
}

char Lexer::peek(int relative_position)
{
    int position = pos + relative_position;
    if (position >= length)
        return '\0';

    return code[position];
}

char Lexer::next()
{
    pos++;
    return peek(0);
}