//
// Created by Xomagat on 09.10.2026.
//

// Bool comes from the compiler as i1, the upper bits of the argument are not defined,
// so every argument is masked with & 1
static inline bool bool_val(unsigned char x) { return (x & 1) != 0; }

// Convert
extern "C" const char* fab_bool_to_str(unsigned char x)    { return (x & 1) ? "true" : "false"; }
extern "C" const char* fab_bool_to_bin(unsigned char x)    { return (x & 1) ? "1" : "0"; }
extern "C" int         fab_bool_to_int(unsigned char x)    { return x & 1; }
extern "C" short       fab_bool_to_short(unsigned char x)  { return (short)(x & 1); }
extern "C" long long   fab_bool_to_long(unsigned char x)   { return (long long)(x & 1); }
extern "C" float       fab_bool_to_float(unsigned char x)  { return (float)(x & 1); }
extern "C" double      fab_bool_to_double(unsigned char x) { return (double)(x & 1); }

// Logic (unlike && and || both sides are always calculated)
extern "C" bool        fab_bool_negate(unsigned char x) { return !(x & 1); }
extern "C" bool        fab_bool_xor(unsigned char x, unsigned char y) { return (x ^ y) & 1; }
extern "C" bool        fab_bool_and(unsigned char x, unsigned char y)     { return bool_val(x) && bool_val(y); }
extern "C" bool        fab_bool_or(unsigned char x, unsigned char y)      { return bool_val(x) || bool_val(y); }
extern "C" bool        fab_bool_nand(unsigned char x, unsigned char y)    { return !(bool_val(x) && bool_val(y)); }
extern "C" bool        fab_bool_nor(unsigned char x, unsigned char y)     { return !(bool_val(x) || bool_val(y)); }
extern "C" bool        fab_bool_xnor(unsigned char x, unsigned char y)    { return bool_val(x) == bool_val(y); }
extern "C" bool        fab_bool_implies(unsigned char x, unsigned char y) { return !bool_val(x) || bool_val(y); }

// Compare: false < true -> -1, 0, 1
extern "C" int         fab_bool_compare(unsigned char x, unsigned char y) { return (int)bool_val(x) - (int)bool_val(y); }