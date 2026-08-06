//
// Created by Xomagat on 06.08.2026.
//

#ifndef FABSHARP_LEXER_H
#define FABSHARP_LEXER_H

#include <string>
#include <vector>

#include "Token.h"
#include "TokenType.h"

class Lexer
{
private:
    std::string OPERATION_CHARS;
    std::vector<token_type> OPERATION_TYPE;

    std::string code;           // code
    std::vector<Token> tokens;  // tokens

    int pos;                    // position now
    int length;                 // code length

    // helpers func
    void add_token(token_type type);
    void add_token(token_type type, std::string text);

    char peek(int relative_position);
    char next();

    void tokenizeNumber();
    void tokenizeOperation();

  public:
    Lexer(std::string code);

    std::vector<Token> tokenize();
};

#endif //FABSHARP_LEXER_H
