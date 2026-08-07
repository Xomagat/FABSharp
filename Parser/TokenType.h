//
// Created by Xomagat on 06.08.2026.
//

#ifndef FABSHARP_TOKENTYPE_H
#define FABSHARP_TOKENTYPE_H

enum token_type
{
    NUMBER,         // 1, 2, 3, 4 ,5 ...
    HEX_NUMBER,     // #101, #12, #645 ...

    PLUS,           // +
    MINUS,          // -
    MULT,           // *
    DIV,            // /

    LPARENT,        // (
    RPARENT,        // )

    eof,            // End of File
};

#endif //FABSHARP_TOKENTYPE_H
