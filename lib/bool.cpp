//
// Created by Xomagat on 09.10.2026.
//

extern "C" const char* fab_bool_to_str(unsigned char x) { return (x & 1) ? "true" : "false"; }
extern "C" int         fab_bool_to_int(unsigned char x) { return x & 1; }
extern "C" bool        fab_bool_negate(unsigned char x) { return !(x & 1); }
extern "C" bool        fab_bool_xor(unsigned char x, unsigned char y) { return (x ^ y) & 1; }