//
// Created by Xomagat on 09.10.2026.
//

#include <ctype.h>

extern "C" bool fab_char_is_digit(char x) { return isdigit(x) != 0; }
extern "C" bool fab_char_is_alpha(char x) { return isalpha(x) != 0; }
extern "C" char fab_char_to_upper(char x) { return toupper(x); }
extern "C" char fab_char_to_lower(char x) { return tolower(x); }