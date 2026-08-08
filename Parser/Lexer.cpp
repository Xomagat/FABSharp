//
// Created by Xomagat on 06.08.2026.
//

#include "Lexer.h"

#include <stdexcept>

// vars

// funcs
Lexer::Lexer(std::string code)
{
    OPERATION_CHARS = "+-*/^()=;";
    OPERATION_TYPE = {
        token_type::PLUS, token_type::MINUS,
        token_type::MULT, token_type::DIV, token_type::POW,
        token_type::LPARENT, token_type::RPARENT,
        token_type::EQ, token_type::SEMI,
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
            tokenize_number();
        }
        else if (current == '"')
        {
            tokenize_string();
        }
        else if (isalpha(current))
        {
            tokenize_word();
        }
        else if (current == '#')
        {
            next();
            tokenize_hex_number();
        }
        else if (OPERATION_CHARS.find(current) != std::string::npos)
        {
            tokenize_operation();
        }
        else
        {
            next(); // whitespaces
        }
    }

    return tokens;
}

void Lexer::tokenize_word()
{
    std::string buffer;
    char current = peek(0);

    while (true)
    {
        if (!isalnum(current) && current != '_')
            break;
        buffer.push_back(current);
        current = next();
    }

    if (buffer == "write")
    {
        add_token(token_type::WRITE);
    }
    else if (buffer == "writeln")
    {
        add_token(token_type::WRITELN);
    }
    else
    {
        add_token(token_type::WORD, buffer);
    }
}

void Lexer::tokenize_string()
{
    next(); // skip "
    std::string buffer;
    char current = peek(0);

    while (current != '"' && current != '\0')
    {
        if (current == '\\')
        {
            current = next();

            switch (current)
            {
            case '"': current = next(); buffer.push_back('"'); continue;
            case 'n': current = next(); buffer.push_back('\n'); continue;
            case 'r': current = next(); buffer.push_back('\r'); continue;
            case 't': current = next(); buffer.push_back('\t'); continue;
            }

            buffer.push_back('\\');
            continue;
        }

        buffer.push_back(current);
        current = next();
    }
    next(); // skip "

    add_token(token_type::TEXT, buffer);
}

void Lexer::tokenize_number()
{
    std::string buffer;
    char current = peek(0);

    while (true)
    {
        if (current == '.')
        {
            if (buffer.find('.') != -1)
                throw std::runtime_error("Incorrect notation of a real number!");
        }
        else if (!isdigit(current))
            break;
        buffer.push_back(current);
        current = next();
    }

    add_token(token_type::NUMBER, buffer);
}

void Lexer::tokenize_hex_number()
{
    std::string buffer, chars = "abcdef";
    char current = peek(0);

    while (isdigit(current) || chars.find(tolower(current)) != -1)
    {
        buffer.push_back(current);
        current = next();
    }

    add_token(token_type::HEX_NUMBER, buffer);
}

void Lexer::tokenize_operation()
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