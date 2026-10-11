//
// Created by Xomagat on 09.10.2026.
//

#include <ctype.h>
#include <stdlib.h>

// The compiler passes char as i8, the upper bits of the argument are not defined,
// so ctype functions always get the value through unsigned char
static inline int uc(char x) { return (unsigned char)x; }

// Check
extern "C" bool fab_char_is_digit(char x)     { return isdigit(uc(x)) != 0; }
extern "C" bool fab_char_is_alpha(char x)     { return isalpha(uc(x)) != 0; }
extern "C" bool fab_char_is_alnum(char x)     { return isalnum(uc(x)) != 0; }
extern "C" bool fab_char_is_upper(char x)     { return isupper(uc(x)) != 0; }
extern "C" bool fab_char_is_lower(char x)     { return islower(uc(x)) != 0; }
extern "C" bool fab_char_is_space(char x)     { return isspace(uc(x)) != 0; }
extern "C" bool fab_char_is_punct(char x)     { return ispunct(uc(x)) != 0; }
extern "C" bool fab_char_is_hex(char x)       { return isxdigit(uc(x)) != 0; }
extern "C" bool fab_char_is_printable(char x) { return isprint(uc(x)) != 0; }
extern "C" bool fab_char_is_ascii(char x)     { return uc(x) < 128; }

// Case
extern "C" char fab_char_to_upper(char x)     { return toupper(uc(x)); }
extern "C" char fab_char_to_lower(char x)     { return tolower(uc(x)); }
extern "C" char fab_char_swap_case(char x)
{
    if (isupper(uc(x))) return tolower(uc(x));
    if (islower(uc(x))) return toupper(uc(x));
    return x;
}

// Convert
extern "C" int       fab_char_to_int(char x)    { return uc(x); }
extern "C" short     fab_char_to_short(char x)  { return (short)uc(x); }
extern "C" long long fab_char_to_long(char x)   { return (long long)uc(x); }
extern "C" float     fab_char_to_float(char x)  { return (float)uc(x); }
extern "C" double    fab_char_to_double(char x) { return (double)uc(x); }

// '0'..'9' -> 0..9, anything else -> -1
extern "C" int fab_char_to_digit(char x)
{
    return (uc(x) >= '0' && uc(x) <= '9') ? uc(x) - '0' : -1;
}

// '0'..'9', 'a'..'f', 'A'..'F' -> 0..15, anything else -> -1
extern "C" int fab_char_to_hex(char x)
{
    if (uc(x) >= '0' && uc(x) <= '9') return uc(x) - '0';
    if (uc(x) >= 'a' && uc(x) <= 'f') return uc(x) - 'a' + 10;
    if (uc(x) >= 'A' && uc(x) <= 'F') return uc(x) - 'A' + 10;
    return -1;
}

extern "C" char* fab_char_to_str(char x)
{
    char* r = (char*)malloc(2);
    if (r == nullptr) return nullptr;

    r[0] = x;
    r[1] = '\0';
    return r;
}

extern "C" char* fab_char_repeat(char x, int n)
{
    if (n < 0) n = 0;

    char* r = (char*)malloc((size_t)n + 1);
    if (r == nullptr) return nullptr;

    for (int i = 0; i < n; i++) r[i] = x;
    r[n] = '\0';
    return r;
}

// Compare: like strcmp (negative, 0, positive)
extern "C" int  fab_char_compare(char x, char y)            { return uc(x) - uc(y); }
extern "C" bool fab_char_equals_ignore_case(char x, char y) { return tolower(uc(x)) == tolower(uc(y)); }