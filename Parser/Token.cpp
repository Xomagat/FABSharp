//
// Created by Xomagat on 06.08.2026.
//

#include "Token.h"

// vars

// funcs
Token::Token()
{
    this->type = token_type::eof;
    this->text = "";
}

Token::Token(token_type type, std::string text)
{
    this->text = text;
    this->type = type;
}

std::string Token::get_text()
{
    return this->text;
}
token_type Token::get_type()
{
    return this->type;
}

void Token::set_text(std::string text)
{
    this->text = text;
}
void Token::set_type(token_type type)
{
    this->type = type;
}