//
// Created by Xomagat on 06.08.2026.
//

#ifndef FABSHARP_TOKENTYPE_H
#define FABSHARP_TOKENTYPE_H

#include <string>
#include <vector>

// Real tokens
enum token_type
{
    NUMBER,         // 1, 2, 3, 4 ,5 ...
    HEX_NUMBER,     // #101, #12, #645 ...
    WORD,           // For const and vars
    TEXT,           // Base string

    // keywords
    WRITE,          // Output command
    WRITELN,        // Output command whit newline
    IF,             // Conditional command (if)
    ELSE,           // Conditional command (else)
    TYPES,          // Types: int, string, bool and another
    WHILE,          // Loop command (while)
    FOR,            // Loop command (for)
    DO,             // Loop command (do)
    BREAK,          // For loop command (break)
    CONTINUE,       // For loop command (continue)

    PLUS,           // +
    MINUS,          // -
    MULT,           // *
    DIV,            // /
    POW,            // ^
    EQ,             // =
    CEQ,            // ==
    NOT,            // !
    NEQ,            // !=
    SEMI,           // ;
    LT,             // <
    GT,             // >
    LTEQ,           // <=
    GTEQ,           // >=
    AMP,            // &
    AND,            // &&
    BAR,            // |
    OR,             // ||

    LPARENT,        // (
    RPARENT,        // )
    LBRACKET,       // {
    RBRACKET,       // }

    eof,            // End of File
};

// Tokens for error
inline std::vector<std::string> tokens_string = {
    "number", "hex_number", "var_id", "text",
    "write", "writeln", "if", "else","type",
    "while", "for", "do", "break", "continue",
    "+", "-", "*", "/",
    "^", "=", "==", "!",
    "!=", ";", "<", ">",
    "<=", ">=", "&&", "||",
    "(", ")", "{", "}",
    "end of file",
};

#endif //FABSHARP_TOKENTYPE_H
