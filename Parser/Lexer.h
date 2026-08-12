//
// Created by Xomagat on 06.08.2026.
//

#ifndef FABSHARP_LEXER_H
#define FABSHARP_LEXER_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include "Token.h"
#include "TokenType.h"

class Lexer
{
private:
    std::string OPERATION_CHARS;
    std::map<std::string, token_type> OPERATORS;

    std::string code;           // code
    std::vector<Token> tokens;  // tokens

    int pos;                    // position now
    int length;                 // code length

    // helpers func
    void add_token(token_type type);
    void add_token(token_type type, std::string text);

    char peek(int relative_position);
    char next();

    void tokenize_number();
    void tokenize_hex_number();
    void tokenize_operation();
    void tokenize_word();
    void tokenize_string();
    void tokenize_comment();
    void tokenize_mcomment();

  public:
    Lexer(std::string code);

    std::vector<Token> tokenize();
};

#endif //FABSHARP_LEXER_H
