//
// Created by Xomagat on 06.08.2026.
//

#include "Lexer.h"

#include <stdexcept>

// vars

// funcs
Lexer::Lexer(std::string code)
{
    OPERATION_CHARS = "+-*/^(){}=;.<>!&|";
    OPERATORS = {
        {"+", token_type::PLUS},
        {"-", token_type::MINUS},
        {"*", token_type::MULT},
        {"/", token_type::DIV},
        {"^", token_type::POW},
        {"=", token_type::EQ},
        {";", token_type::SEMI},
        {".", token_type::COMMA},

        {"{", token_type::LBRACKET},
        {"}", token_type::RBRACKET},
        {"(", token_type::LPARENT},
        {")", token_type::RPARENT},

        {"!", token_type::NOT},
        {"&", token_type::AMP},
        {"|", token_type::BAR},

        {"==", token_type::CEQ},
        {"!=", token_type::NEQ},
        {"<", token_type::LT},
        {">", token_type::GT},
        {"<=", token_type::LTEQ},
        {">=", token_type::GTEQ},

        {"&&", token_type::AND},
        {"||", token_type::OR},
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

    static const std::unordered_map<std::string, token_type> keywords = {
        {"write",   token_type::WRITE},
        {"writeln", token_type::WRITELN},
        {"if",      token_type::IF},
        {"else",    token_type::ELSE},
        {"while",   token_type::WHILE},
        {"for",     token_type::FOR},
        {"do",      token_type::DO},
        {"break",   token_type::BREAK},
        {"continue",token_type::CONTINUE},
        {"define",  token_type::DEFINE},
        {"return",  token_type::RETURN},
        {"int",     token_type::TYPES},
        {"double",  token_type::TYPES},
        {"float",   token_type::TYPES},
        {"short",   token_type::TYPES},
        {"long",    token_type::TYPES},
        {"byte",    token_type::TYPES},
        {"string",  token_type::TYPES},
        {"bool",    token_type::TYPES},
    };

    auto it = keywords.find(buffer);
    if (it != keywords.end())
    {
        add_token(it->second, buffer);
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
    if (peek(0) == '/')
    {
        if (peek(1) == '/') { next(); next(); tokenize_comment(); return; }
        if (peek(1) == '*') { next(); next(); tokenize_mcomment(); return; }
    }

    std::string one(1, peek(0));
    std::string two = one + peek(1);

    if (OPERATORS.contains(two))
    {
        add_token(OPERATORS.at(two));
        next(); next();
        return;
    }

    if (OPERATORS.contains(one))
    {
        add_token(OPERATORS.at(one));
        next();
        return;
    }

    throw std::runtime_error("Unknown operator: " + one);
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

void Lexer::tokenize_comment()
{
    char current = peek(0);

    while (current != '\n' && current != '\0')
    {
        current = next();
    }
}

void Lexer::tokenize_mcomment()
{
    char current = peek(0);

    while (true)
    {
        if (current == '\0') throw std::runtime_error("Missing close tag: '*/'!");
        if (current == '*' && peek(1) == '/') break;
        current = next();
    }

    next(); next();
}