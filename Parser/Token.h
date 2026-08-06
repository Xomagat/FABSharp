//
// Created by Xomagat on 06.08.2026.
//

#ifndef FABSHARP_TOKEN_H
#define FABSHARP_TOKEN_H

#include <string>

#include "TokenType.h"

class Token
{
private:
    std::string text;   // value
    token_type type;     // type

public:
    // constructors
    Token();
    Token(token_type type, std::string text);

    // getters
    std::string get_text();
    token_type get_type();

    // setters
    void set_text(std::string text);
    void set_type(token_type type);
};


#endif //FABSHARP_TOKEN_H
