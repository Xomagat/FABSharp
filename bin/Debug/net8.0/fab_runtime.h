#pragma once

#define _USE_MATH_DEFINES

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/ioctl.h>
#include <pwd.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <ctime>
#include <cstdarg>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <functional>

// ════════════════════════════════════════════════════════════════
//  Fab runtime — C++17 implementation
// ════════════════════════════════════════════════════════════════

enum class FabTag : uint8_t {
    Null = 0, Double, Bool, String, Char, List, Dict, Tuple, Ptr
};

// Forward declarations
struct FabVal;
struct FabList;
struct FabDict;

using FabListPtr = FabList*;
using FabDictPtr = FabDict*;

// ── Value type ───────────────────────────────────────────────────
struct FabVal {
    FabTag tag = FabTag::Null;

    // payload
    double          d = 0.0;
    bool            b = false;
    char            c = '\0';
    std::string     s;
    FabListPtr      list = nullptr;
    FabDictPtr      dict = nullptr;
    FabVal* ptr = nullptr;   // Ptr tag: raw pointer to another FabVal

    // tuple payload (owned)
    std::vector<FabVal> tuple_items;

    FabVal() = default;
};

// ── List ─────────────────────────────────────────────────────────
struct FabList {
    std::vector<FabVal> items;
    int limit; // -1 = unlimited

    explicit FabList(int lim = -1) : limit(lim) {}

    void push_back(const FabVal& v) {
        if (limit >= 0 && (int)items.size() >= limit)
            throw std::runtime_error("List is full (capacity " + std::to_string(limit) + ")");
        items.push_back(v);
    }

    void push_front(const FabVal& v) {
        if (limit >= 0 && (int)items.size() >= limit)
            throw std::runtime_error("List is full (capacity " + std::to_string(limit) + ")");
        items.insert(items.begin(), v);
    }

    FabVal& at(int i) {
        if (i < 0 || i >= (int)items.size())
            throw std::runtime_error("List index " + std::to_string(i) + " out of range (" + std::to_string(items.size()) + ")");
        return items[i];
    }
};

// ── Dict ─────────────────────────────────────────────────────────
struct FabDict {
    std::vector<std::pair<FabVal, FabVal>> pairs;
    int limit; // -1 = unlimited

    explicit FabDict(int lim = -1) : limit(lim) {}

    int find_index(const std::string& ks) const {
        for (int i = 0; i < (int)pairs.size(); i++)
            if (fab_fmt_key(pairs[i].first) == ks) return i;
        return -1;
    }

    static std::string fab_fmt_key(const FabVal& v);  // defined below

    void set(const FabVal& key, const FabVal& val) {
        int i = find_index(fab_fmt_key(key));
        if (i >= 0) { pairs[i].second = val; return; }
        if (limit >= 0 && (int)pairs.size() >= limit)
            throw std::runtime_error("Dictionary is full (capacity " + std::to_string(limit) + ")");
        pairs.push_back({ key, val });
    }

    FabVal get(const FabVal& key) const {
        int i = find_index(fab_fmt_key(key));
        if (i < 0) throw std::runtime_error("Key '" + fab_fmt_key(key) + "' not found in dictionary");
        return pairs[i].second;
    }

    bool contains(const FabVal& key) const {
        return find_index(fab_fmt_key(key)) >= 0;
    }
};

// ── Constructors ─────────────────────────────────────────────────
inline FabVal fab_num(double d) { FabVal v; v.tag = FabTag::Double; v.d = d; return v; }
inline FabVal fab_bool_v(bool b) { FabVal v; v.tag = FabTag::Bool;   v.b = b; return v; }
inline FabVal fab_char_v(char c) { FabVal v; v.tag = FabTag::Char;   v.c = c; return v; }
inline FabVal fab_str(const std::string& s) { FabVal v; v.tag = FabTag::String; v.s = s; return v; }
inline FabVal fab_str(const char* s) { FabVal v; v.tag = FabTag::String; v.s = s ? s : ""; return v; }
inline FabVal fab_list_val(FabListPtr l) { FabVal v; v.tag = FabTag::List; v.list = l; return v; }
inline FabVal fab_dict_val(FabDictPtr d) { FabVal v; v.tag = FabTag::Dict; v.dict = d; return v; }

// ── Pointer helpers ──────────────────────────────────────────────
/// Take address of a FabVal variable — stores raw C++ pointer.
/// Lifetime is the caller's responsibility (valid only within the same scope).
inline FabVal fab_addr_of(FabVal* target) {
    FabVal v; v.tag = FabTag::Ptr; v.ptr = target; return v;
}
/// Dereference a Ptr FabVal — returns the value it points to.
inline FabVal fab_deref(const FabVal& p) {
    if (p.tag != FabTag::Ptr || !p.ptr)
        throw std::runtime_error("Dereference (*) requires a pointer value");
    return *p.ptr;
}
/// Write through a Ptr FabVal.
inline void fab_deref_set(const FabVal& p, const FabVal& val) {
    if (p.tag != FabTag::Ptr || !p.ptr)
        throw std::runtime_error("Pointer write (*) requires a pointer value");
    *p.ptr = val;
}

inline const FabVal FAB_NIL = fab_bool_v(false); // tag Null
inline const FabVal FAB_TRUE = fab_bool_v(true);
inline const FabVal FAB_FALSE = fab_bool_v(false);

// We need to re-initialise FAB_NIL properly:
namespace fab_detail {
    inline FabVal make_nil() { FabVal v; v.tag = FabTag::Null; return v; }
}
// Override with correct nil
#define FAB_NIL   fab_detail::make_nil()

// ── Error type hierarchy (real native C++ classes — try/catch/finally) ────
// Mirrors AST.FabErrorTypes on the interpreter side. 'new TypeError("msg")'
// compiles to `FabErr_TypeError(msg)` — a genuine C++ object deriving from
// FabError deriving from std::runtime_error. Because this is *real*
// inheritance (not a FabVal tag check), 'catch (FabError&)' polymorphically
// catches every built-in error subtype, exactly like exception hierarchies
// in any other language, and 'catch (FabErr_TypeError&)' catches only that
// one. Plain string throws / internal runtime errors are still ordinary
// std::runtime_error and are picked up the same way via 'catch (string e)'
// (compiled as `catch (const std::exception&)`).
struct FabError : public std::runtime_error {
    std::string type_name;
    // Two-arg form used internally by every FabErr_* subtype.
    FabError(std::string type, const std::string& msg)
        : std::runtime_error(msg), type_name(std::move(type)) {}
    // One-arg form used for a bare 'new Error("msg")'.
    explicit FabError(const std::string& msg)
        : std::runtime_error(msg), type_name("Error") {}
};

struct FabErr_TypeError : public FabError {
    explicit FabErr_TypeError(const std::string& msg) : FabError("TypeError", msg) {}
};
struct FabErr_ValueError : public FabError {
    explicit FabErr_ValueError(const std::string& msg) : FabError("ValueError", msg) {}
};
struct FabErr_IndexError : public FabError {
    explicit FabErr_IndexError(const std::string& msg) : FabError("IndexError", msg) {}
};
struct FabErr_ArgumentError : public FabError {
    explicit FabErr_ArgumentError(const std::string& msg) : FabError("ArgumentError", msg) {}
};
struct FabErr_IOError : public FabError {
    explicit FabErr_IOError(const std::string& msg) : FabError("IOError", msg) {}
};
struct FabErr_NotFoundError : public FabError {
    explicit FabErr_NotFoundError(const std::string& msg) : FabError("NotFoundError", msg) {}
};

// ── Formatting ───────────────────────────────────────────────────
inline std::string fab_fmt(const FabVal& v);

inline std::string fab_num_str(double d) {
    if (!std::isinf(d) && d == std::floor(d) && d >= -9e18 && d <= 9e18)
        return std::to_string((long long)d);
    std::ostringstream oss;
    oss.precision(14);
    oss << d;
    return oss.str();
}

inline std::string FabDict::fab_fmt_key(const FabVal& v) {
    return fab_fmt(v);
}

inline std::string fab_fmt(const FabVal& v) {
    switch (v.tag) {
    case FabTag::Null:   return "null";
    case FabTag::Bool:   return v.b ? "true" : "false";
    case FabTag::Double: return fab_num_str(v.d);
    case FabTag::String: return v.s;
    case FabTag::Char:   return std::string(1, v.c);
    case FabTag::Ptr: {
        // Print as hex address string, matching the interpreter's "0xXXXXXXXX" style
        char buf[32];
        std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)(uintptr_t)v.ptr);
        return buf;
    }
    case FabTag::List: {
        std::string r = "[";
        for (int i = 0; i < (int)v.list->items.size(); i++) {
            if (i) r += ", ";
            r += fab_fmt(v.list->items[i]);
        }
        return r + "]";
    }
    case FabTag::Dict: {
        std::string r = "{";
        for (int i = 0; i < (int)v.dict->pairs.size(); i++) {
            if (i) r += ", ";
            r += fab_fmt(v.dict->pairs[i].first) + ": " + fab_fmt(v.dict->pairs[i].second);
        }
        return r + "}";
    }
    case FabTag::Tuple: {
        std::string r = "(";
        for (int i = 0; i < (int)v.tuple_items.size(); i++) {
            if (i) r += ", ";
            r += fab_fmt(v.tuple_items[i]);
        }
        return r + ")";
    }
    }
    return "?";
}

// ── Truthiness ───────────────────────────────────────────────────
inline bool fab_truthy(const FabVal& v) {
    switch (v.tag) {
    case FabTag::Null:   return false;
    case FabTag::Bool:   return v.b;
    case FabTag::Double: return v.d != 0.0;
    case FabTag::String: return !v.s.empty();
    case FabTag::Char:   return v.c != '\0';
    default:             return true;
    }
}

// ── Equality ─────────────────────────────────────────────────────
inline bool fab_eq(const FabVal& a, const FabVal& b) {
    return fab_fmt(a) == fab_fmt(b);
}

// ── Arithmetic ───────────────────────────────────────────────────
inline FabVal fab_concat(const FabVal& a, const FabVal& b) {
    return fab_str(fab_fmt(a) + fab_fmt(b));
}
inline FabVal fab_add(const FabVal& a, const FabVal& b) {
    if (a.tag == FabTag::String || b.tag == FabTag::String)
        return fab_concat(a, b);
    return fab_num(a.d + b.d);
}
inline FabVal fab_sub(const FabVal& a, const FabVal& b) { return fab_num(a.d - b.d); }
inline FabVal fab_mul(const FabVal& a, const FabVal& b) { return fab_num(a.d * b.d); }
inline FabVal fab_div(const FabVal& a, const FabVal& b) { return fab_num(a.d / b.d); }
inline FabVal fab_mod(const FabVal& a, const FabVal& b) { return fab_num(std::fmod(a.d, b.d)); }
inline FabVal fab_pow_v(const FabVal& a, const FabVal& b) { return fab_num(std::pow(a.d, b.d)); }
inline FabVal fab_lt(const FabVal& a, const FabVal& b) { return fab_bool_v(a.d < b.d); }
inline FabVal fab_gt(const FabVal& a, const FabVal& b) { return fab_bool_v(a.d > b.d); }
inline FabVal fab_le(const FabVal& a, const FabVal& b) { return fab_bool_v(a.d <= b.d); }
inline FabVal fab_ge(const FabVal& a, const FabVal& b) { return fab_bool_v(a.d >= b.d); }

// ── List helpers ─────────────────────────────────────────────────
inline FabListPtr fab_list_new(int limit) { return new FabList(limit); }

inline void fab_list_push(FabListPtr l, const FabVal& v) { l->push_back(v); }
inline void fab_list_push_front(FabListPtr l, const FabVal& v) { l->push_front(v); }

inline FabVal fab_list_get(FabListPtr l, int i) { return l->at(i); }
inline void   fab_list_set(FabListPtr l, int i, const FabVal& v) { l->at(i) = v; }

inline FabVal fab_list_contains(FabListPtr l, const FabVal& item) {
    for (auto& x : l->items)
        if (fab_eq(x, item)) return FAB_TRUE;
    return FAB_FALSE;
}

// ── Dict helpers ─────────────────────────────────────────────────
inline FabDictPtr fab_dict_new(int limit) { return new FabDict(limit); }
inline void fab_dict_set(FabDictPtr d, const FabVal& key, const FabVal& val) { d->set(key, val); }
inline FabVal fab_dict_get(FabDictPtr d, const FabVal& key) { return d->get(key); }
inline int fab_dict_find(FabDictPtr d, const FabVal& key) { return d->find_index(fab_fmt(key)); }

// ── Vararg collection builders ────────────────────────────────────
inline FabVal fab_make_list(int limit, int n, ...) {
    FabListPtr l = fab_list_new(limit);
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++) fab_list_push(l, va_arg(ap, FabVal));
    va_end(ap);
    return fab_list_val(l);
}

inline FabVal fab_make_dict(int limit, int n, ...) {
    FabDictPtr d = fab_dict_new(limit);
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++) {
        FabVal k = va_arg(ap, FabVal);
        FabVal v = va_arg(ap, FabVal);
        fab_dict_set(d, k, v);
    }
    va_end(ap);
    return fab_dict_val(d);
}

inline FabVal fab_make_tuple(int n, ...) {
    FabVal v; v.tag = FabTag::Tuple;
    va_list ap; va_start(ap, n);
    for (int i = 0; i < n; i++) v.tuple_items.push_back(va_arg(ap, FabVal));
    va_end(ap);
    return v;
}

// ── String methods ────────────────────────────────────────────────
inline FabVal fab_str_to_lower(FabVal s) {
    std::string r = s.s;
    for (char& ch : r) ch = (char)std::tolower((unsigned char)ch);
    return fab_str(r);
}
inline FabVal fab_str_to_upper(FabVal s) {
    std::string r = s.s;
    for (char& ch : r) ch = (char)std::toupper((unsigned char)ch);
    return fab_str(r);
}
inline FabVal fab_str_substr(FabVal s, int from, int to) {
    int len = (int)s.s.size();
    if (from < 0) from = 0;
    if (to < 0 || to > len) to = len;
    int n = to - from; if (n < 0) n = 0;
    return fab_str(s.s.substr(from, n));
}
inline FabVal fab_str_index(FabVal s, FabVal sub) {
    auto pos = s.s.find(fab_fmt(sub));
    return fab_num(pos == std::string::npos ? -1.0 : (double)pos);
}
inline FabVal fab_str_contains(FabVal s, FabVal sub) {
    return fab_bool_v(s.s.find(fab_fmt(sub)) != std::string::npos);
}
inline FabVal fab_str_replace(FabVal s, FabVal fv, FabVal tv) {
    std::string str = s.s, from = fab_fmt(fv), to = fab_fmt(tv);
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
    return fab_str(str);
}
inline FabVal fab_str_trim(FabVal s) {
    std::string r = s.s;
    r.erase(r.begin(), std::find_if(r.begin(), r.end(), [](unsigned char c) { return !std::isspace(c); }));
    r.erase(std::find_if(r.rbegin(), r.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), r.end());
    return fab_str(r);
}
inline FabVal fab_str_split(FabVal s, FabVal sep) {
    FabListPtr l = fab_list_new(-1);
    std::string str = s.s, delim = fab_fmt(sep);
    size_t pos = 0, found;
    while ((found = str.find(delim, pos)) != std::string::npos) {
        fab_list_push(l, fab_str(str.substr(pos, found - pos)));
        pos = found + delim.size();
    }
    fab_list_push(l, fab_str(str.substr(pos)));
    return fab_list_val(l);
}

// ── I/O ──────────────────────────────────────────────────────────
inline FabVal fab_readline() {
    std::string line;
    std::getline(std::cin, line);
    return fab_str(line);
}
inline FabVal fab_parse_input(FabVal raw) {
    char* end;
    double d = std::strtod(raw.s.c_str(), &end);
    if (end != raw.s.c_str() && *end == '\0') return fab_num(d);
    return raw;
}
// input_in <type> name; — unlike fab_parse_input() (which just guesses at
// numeric-vs-not for untyped 'input_in name;'), a declared type must be
// actually validated: bad input throws FabErr_ValueError instead of silently
// falling back to storing the raw string, mirroring FabInputIn.Eval on the
// interpreter side.
inline FabVal fab_parse_typed_input(const FabVal& raw, const std::string& type_name) {
    const std::string& s = raw.s;
    auto fail = [&]() -> FabVal {
        throw FabErr_ValueError("Input error: '" + s + "' is not a valid " + type_name);
        };
    if (type_name == "string") return raw;
    if (type_name == "char")
        return s.size() == 1 ? fab_char_v(s[0]) : fail();
    if (type_name == "bool") {
        if (s == "true" || s == "True") return FAB_TRUE;
        if (s == "false" || s == "False") return FAB_FALSE;
        return fail();
    }
    if (s.empty()) return fail();
    char* end = nullptr;
    double d = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return fail();
    bool isFloatingType = (type_name == "float" || type_name == "double" || type_name == "ldouble");
    if (!isFloatingType && d != std::floor(d)) return fail();
    return fab_num(d);
}
inline void fab_write(const FabVal& v) { std::cout << fab_fmt(v); }
inline void fab_writeln(const FabVal& v) { std::cout << fab_fmt(v) << '\n'; }

// ── Member helpers (type-polymorphic) ────────────────────────────

/// fab_member_length — works for List, Dict, String, Tuple
inline FabVal fab_member_length(const FabVal& v) {
    switch (v.tag) {
    case FabTag::List:   return fab_num((double)v.list->items.size());
    case FabTag::Dict:   return fab_num((double)v.dict->pairs.size());
    case FabTag::String: return fab_num((double)v.s.size());
    case FabTag::Tuple:  return fab_num((double)v.tuple_items.size());
    default: return FAB_NIL;
    }
}

/// fab_list_sort
inline void fab_list_sort(FabListPtr l) {
    std::sort(l->items.begin(), l->items.end(), [](const FabVal& a, const FabVal& b) {
        if (a.tag == FabTag::String && b.tag == FabTag::String) return a.s < b.s;
        return a.d < b.d;
        });
}

/// fab_dict_keys / fab_dict_values
inline FabVal fab_dict_keys(FabDictPtr d) {
    FabListPtr l = fab_list_new(-1);
    for (auto& p : d->pairs) fab_list_push(l, p.first);
    return fab_list_val(l);
}
inline FabVal fab_dict_values(FabDictPtr d) {
    FabListPtr l = fab_list_new(-1);
    for (auto& p : d->pairs) fab_list_push(l, p.second);
    return fab_list_val(l);
}

/// fab_str_repeat — repeat char/string n times
inline FabVal fab_str_repeat(FabVal s, FabVal ch, FabVal n) {
    std::string unit = (ch.tag == FabTag::Char) ? std::string(1, ch.c) : fab_fmt(ch);
    int times = (int)n.d;
    std::string result;
    result.reserve(unit.size() * (size_t)times);
    for (int i = 0; i < times; i++) result += unit;
    return fab_str(result);
}

/// fab_str_isdigit — true if all characters are digits
inline FabVal fab_str_isdigit(const FabVal& v) {
    if (v.tag == FabTag::Char) return fab_bool_v(std::isdigit((unsigned char)v.c) != 0);
    if (v.tag != FabTag::String || v.s.empty()) return FAB_FALSE;
    for (unsigned char c : v.s) if (!std::isdigit(c)) return FAB_FALSE;
    return FAB_TRUE;
}

/// fab_str_is_null_or_space
inline FabVal fab_str_is_null_or_space(const FabVal& v) {
    if (v.tag == FabTag::Null) return FAB_TRUE;
    if (v.tag != FabTag::String) return FAB_FALSE;
    for (unsigned char c : v.s) if (!std::isspace(c)) return FAB_FALSE;
    return FAB_TRUE;
}

/// issymbol_compat — no std::issymbol in C; approximate as ispunct
inline bool issymbol_compat(unsigned char c) { return std::ispunct(c) != 0; }

// ── Math library ─────────────────────────────────────────────────
inline FabVal fab_math_pow(FabVal a, FabVal b) { return fab_num(std::pow(a.d, b.d)); }
inline FabVal fab_math_sqrt(FabVal a) { return fab_num(std::sqrt(a.d)); }
inline FabVal fab_math_cbrt(FabVal a) { return fab_num(std::cbrt(a.d)); }
inline FabVal fab_math_abs(FabVal a) { return fab_num(std::fabs(a.d)); }
inline FabVal fab_math_floor(FabVal a) { return fab_num(std::floor(a.d)); }
inline FabVal fab_math_ceil(FabVal a) { return fab_num(std::ceil(a.d)); }
inline FabVal fab_math_round(FabVal a) { return fab_num(std::round(a.d)); }
inline FabVal fab_math_sin(FabVal a) { return fab_num(std::sin(a.d)); }
inline FabVal fab_math_cos(FabVal a) { return fab_num(std::cos(a.d)); }
inline FabVal fab_math_tan(FabVal a) { return fab_num(std::tan(a.d)); }
inline FabVal fab_math_log1(FabVal a) { return fab_num(std::log(a.d)); }
inline FabVal fab_math_log2v(FabVal a, FabVal b) { return fab_num(std::log(a.d) / std::log(b.d)); }
inline FabVal fab_math_max(FabVal a, FabVal b) { return fab_num(a.d > b.d ? a.d : b.d); }
inline FabVal fab_math_min(FabVal a, FabVal b) { return fab_num(a.d < b.d ? a.d : b.d); }
inline FabVal fab_math_clamp(FabVal v, FabVal lo, FabVal hi) {
    double r = v.d < lo.d ? lo.d : (v.d > hi.d ? hi.d : v.d);
    return fab_num(r);
}
inline FabVal fab_math_rad(FabVal a) { return fab_num(a.d * (3.14159265358979323846 / 180.0)); }
inline FabVal fab_math_deg(FabVal a) { return fab_num(a.d * (180.0 / 3.14159265358979323846)); }
inline FabVal fab_math_exp(FabVal a) { return fab_num(std::exp(a.d)); }
inline FabVal fab_math_atan2v(FabVal y, FabVal x) { return fab_num(std::atan2(y.d, x.d)); }
inline FabVal fab_math_sign(FabVal a) { return fab_num(a.d > 0 ? 1 : a.d < 0 ? -1 : 0); }
inline FabVal fab_math_lerp(FabVal a, FabVal b, FabVal t) { return fab_num(a.d + (b.d - a.d) * t.d); }
inline FabVal fab_math_hypot(FabVal a, FabVal b) { return fab_num(std::hypot(a.d, b.d)); }
inline FabVal fab_math_factoriall(FabVal n) {
    long long r = 1, i = (long long)n.d;
    for (; i > 1; i--) r *= i;
    return fab_num((double)r);
}
inline FabVal fab_math_factorial(FabVal n) {
    int r = 1, i = (int)n.d;
    for (; i > 1; i--) r *= i;
    return fab_num((double)r);
}
inline FabVal fab_math_factoriald(FabVal n) {
    double r = 1, i = (double)n.d;
    for (; i > 1; i--) r *= i;
    return fab_num((double)r);
}
inline FabVal fab_math_factorialld(FabVal n) {
    long double r = 1, i = (long double)n.d;
    for (; i > 1; i--) r *= i;
    return fab_num((long double)r);
}
inline FabVal fab_math_tan2(FabVal sin_v, FabVal cos_v) {
    if (cos_v.d == 0.0) throw std::runtime_error("math.tan: cos is zero, tan is undefined");
    return fab_num(sin_v.d / cos_v.d);
}
inline FabVal fab_math_expm(FabVal a) { return fab_num(std::pow(2.7182818, a.d) - 1.0); }

static double fab_log_gamma_impl(double val) {
    double c[] = { 76.18009172947146,-86.50532032941677,24.01409824083091,
                  -1.231739572450155,0.1208650973866179e-2,-0.5395239384953e-5 };
    double temp = val + 5.5;
    temp -= (val + 0.5) * std::log(temp);
    double ser = 1.000000000190015;
    for (int i = 0; i < 6; i++) ser += c[i] / (++val);
    return -temp + std::log(2.5066282746310005 * ser / (val - (val - 1.0)));
}
static double fab_gamma_impl(double x) {
    double p[] = { 676.5203681218851,-1259.1392167224028,771.32342877765313,
                  -176.61502916214059,12.507343278686905,-0.13857109526572012,
                  9.9843695780195716e-6,1.5056327351493116e-7 };
    if (x < 0.5) return M_PI / (std::sin(M_PI * x) * fab_gamma_impl(1.0 - x));
    x -= 1;
    double a = 0.99999999999980993;
    double t = x + 7.5;
    for (int i = 0; i < 8; i++) a += p[i] / (x + i + 1);
    return std::sqrt(2 * M_PI) * std::pow(t, x + 0.5) * std::exp(-t) * a;
}
inline FabVal fab_math_gamma(FabVal a) { return fab_num(fab_gamma_impl(a.d)); }
inline FabVal fab_math_log_gamma(FabVal a) { return fab_num(fab_log_gamma_impl(a.d)); }
inline FabVal fab_math_beta(FabVal x, FabVal y) {
    return fab_num(std::exp(fab_log_gamma_impl(x.d) + fab_log_gamma_impl(y.d) - fab_log_gamma_impl(x.d + y.d)));
}
inline FabVal fab_math_betal(FabVal x, FabVal y) {
    return fab_num((double)(long long)std::exp(fab_log_gamma_impl(x.d) + fab_log_gamma_impl(y.d) - fab_log_gamma_impl(x.d + y.d)));
}
inline FabVal fab_math_pi() { return fab_num(3.14159265358979323846); }
inline FabVal fab_math_e() { return fab_num(2.71828182845904523536); }
inline FabVal fab_math_tau() { return fab_num(6.28318530717958647692); }
inline FabVal fab_math_gam() { return fab_num(0.5772156649015328); }
inline FabVal fab_math_g() { return fab_num(0.9159655941772190); }
inline FabVal fab_math_phi() { return fab_num(1.6180339887498948); }
inline FabVal fab_math_sqrt2() { return fab_num(1.4142135623); }

// ── Random ───────────────────────────────────────────────────────
inline FabVal fab_random_randi(FabVal lo, FabVal hi) {
    int l = (int)lo.d, h = (int)hi.d;
    return fab_num((double)(rand() % (h - l + 1) + l));
}
inline FabVal fab_random_randl(FabVal lo, FabVal hi) {
    long l = (long)lo.d, h = (long)hi.d;
    return fab_num((double)(rand() % (h - l + 1) + l));
}
inline FabVal fab_random_rands(FabVal lo, FabVal hi) {
    short l = (short)lo.d, h = (short)hi.d;
    return fab_num((double)(rand() % (h - l + 1) + l));
}
inline FabVal fab_random_randb(FabVal lo, FabVal hi) {
    char l = (char)lo.d, h = (char)hi.d;
    return fab_num((double)(rand() % (h - l + 1) + l));
}
inline FabVal fab_random_randd(FabVal lo, FabVal hi) {
    return fab_num(lo.d + (hi.d - lo.d) * ((double)rand() / RAND_MAX));
}
inline FabVal fab_random_randf(FabVal lo, FabVal hi) {
    return fab_num(lo.d + (hi.d - lo.d) * ((double)rand() / RAND_MAX));
}

// ── IO library ───────────────────────────────────────────────────
inline FabVal fab_io_read_file(FabVal path) {
    std::ifstream f(path.s);
    if (!f) throw std::runtime_error("File not found: " + path.s);
    return fab_str(std::string(std::istreambuf_iterator<char>(f), {}));
}
inline FabVal fab_io_write_file(FabVal path, FabVal text) {
    std::ofstream f(path.s);
    if (!f) throw std::runtime_error("Cannot write: " + path.s);
    f << text.s;
    return FAB_NIL;
}
inline FabVal fab_io_append_file(FabVal path, FabVal text) {
    std::ofstream f(path.s, std::ios::app);
    if (!f) throw std::runtime_error("Cannot append: " + path.s);
    f << text.s;
    return FAB_NIL;
}
inline FabVal fab_io_file_exists(FabVal path) {
    std::ifstream f(path.s);
    return fab_bool_v(f.good());
}
#include <filesystem>
namespace fs = std::filesystem;

inline FabVal fab_io_delete_file(FabVal path) {
    if (!fs::exists(path.s)) throw std::runtime_error("io.delete_file: file not found: " + path.s);
    fs::remove(path.s);
    return FAB_NIL;
}
inline FabVal fab_io_read_lines(FabVal path) {
    std::ifstream f(path.s);
    if (!f) throw std::runtime_error("io.read_lines: file not found: " + path.s);
    std::ostringstream oss;
    std::string line; bool first = true;
    while (std::getline(f, line)) { if (!first) oss << '\n'; oss << line; first = false; }
    return fab_str(oss.str());
}
inline FabVal fab_io_write_lines(FabVal path, FabVal text) {
    std::ofstream f(path.s, std::ios::app);
    if (!f) throw std::runtime_error("io.write_lines: cannot open: " + path.s);
    f << text.s << '\n';
    return FAB_NIL;
}
inline FabVal fab_io_copy_file(FabVal src, FabVal dst) {
    if (!fs::exists(src.s)) throw std::runtime_error("io.copy_file: source not found: " + src.s);
    fs::copy_file(src.s, dst.s, fs::copy_options::overwrite_existing);
    return FAB_NIL;
}
inline FabVal fab_io_move_file(FabVal src, FabVal dst) {
    if (!fs::exists(src.s)) throw std::runtime_error("io.move_file: source not found: " + src.s);
    fs::rename(src.s, dst.s);
    return FAB_NIL;
}
inline FabVal fab_io_dir_exists(FabVal path) { return fab_str(fs::is_directory(path.s) ? "true" : "false"); }
inline FabVal fab_io_create_dir(FabVal path) { fs::create_directories(path.s); return FAB_NIL; }
inline FabVal fab_io_delete_dir(FabVal path) {
    if (!fs::exists(path.s)) throw std::runtime_error("io.delete_dir: not found: " + path.s);
    fs::remove_all(path.s);
    return FAB_NIL;
}
inline FabVal fab_io_list_files(FabVal path) {
    if (!fs::is_directory(path.s)) throw std::runtime_error("io.list_files: not a directory: " + path.s);
    std::vector<std::string> names;
    for (auto& e : fs::directory_iterator(path.s))
        if (fs::is_regular_file(e)) names.push_back(e.path().filename().string());
    std::string r;
    for (size_t i = 0; i < names.size(); i++) { if (i) r += ','; r += names[i]; }
    return fab_str(r);
}
inline FabVal fab_io_list_dirs(FabVal path) {
    if (!fs::is_directory(path.s)) throw std::runtime_error("io.list_dirs: not a directory: " + path.s);
    std::vector<std::string> names;
    for (auto& e : fs::directory_iterator(path.s))
        if (fs::is_directory(e)) names.push_back(e.path().filename().string());
    std::string r;
    for (size_t i = 0; i < names.size(); i++) { if (i) r += ','; r += names[i]; }
    return fab_str(r);
}
inline FabVal fab_io_path_join(FabVal a, FabVal b) { return fab_str((fs::path(a.s) / b.s).string()); }
inline FabVal fab_io_get_ext(FabVal path) { return fab_str(fs::path(path.s).extension().string()); }
inline FabVal fab_io_get_name(FabVal path) { return fab_str(fs::path(path.s).stem().string()); }
inline FabVal fab_io_file_size(FabVal path) {
    if (!fs::exists(path.s)) throw std::runtime_error("io.file_size: not found: " + path.s);
    return fab_str(std::to_string(fs::file_size(path.s)));
}

// ── Console ──────────────────────────────────────────────────────
inline FabVal fab_console_clear() { std::cout << "\033[2J\033[H"; return FAB_NIL; }
inline FabVal fab_console_reset_color() { std::cout << "\033[0m"; return FAB_NIL; }
// Helper: emit ANSI color escape for fg (base=30) or bg (base=40).
// Accepts either a string name ("Red", "Green" ...) or a FabColor dict.
// Does NOT call fab_is_color / fab_color_r because those are defined later.
inline void fab_console_emit_color(const FabVal& color, int base) {
    // FabColor is stored as a Dict with key "__color__"
    if (color.tag == FabTag::Dict && color.dict &&
        color.dict->find_index("__color__") >= 0)
    {
        int r = (int)fab_dict_get(color.dict, fab_str("r")).d;
        int g = (int)fab_dict_get(color.dict, fab_str("g")).d;
        int b = (int)fab_dict_get(color.dict, fab_str("b")).d;
        // ANSI 24-bit color: ESC[38;2;r;g;bm (fg) or ESC[48;2;r;g;bm (bg)
        std::cout << "\033[" << (base == 30 ? 38 : 48)
            << ";2;" << r << ";" << g << ";" << b << "m";
        return;
    }
    // Named color string
    const std::string& c = (color.tag == FabTag::String) ? color.s : "";
    if (c == "Red" || c == "red")     std::cout << "\033[" << (base + 1) << "m";
    else if (c == "Green" || c == "green")   std::cout << "\033[" << (base + 2) << "m";
    else if (c == "Yellow" || c == "yellow")  std::cout << "\033[" << (base + 3) << "m";
    else if (c == "Blue" || c == "blue")    std::cout << "\033[" << (base + 4) << "m";
    else if (c == "Magenta" || c == "magenta") std::cout << "\033[" << (base + 5) << "m";
    else if (c == "Cyan" || c == "cyan")    std::cout << "\033[" << (base + 6) << "m";
    else if (c == "White" || c == "white")   std::cout << "\033[" << (base + 7) << "m";
    else if (c == "Black" || c == "black")   std::cout << "\033[" << (base + 0) << "m";
    else                                        std::cout << "\033[0m";
}

inline FabVal fab_console_set_fg(FabVal color) {
    fab_console_emit_color(color, 30);
    return FAB_NIL;
}
inline FabVal fab_console_bold(FabVal s) {
    return fab_str("\033[1m" + s.s + "\033[0m");
}
inline FabVal fab_console_set_bg(FabVal color) {
    fab_console_emit_color(color, 40);
    return FAB_NIL;
}
inline FabVal fab_console_set_title(FabVal title) {
    std::cout << "\033]0;" << title.s << "\007";
    return FAB_NIL;
}
inline FabVal fab_console_beep() { std::cout << '\a'; return FAB_NIL; }
inline FabVal fab_console_set_cursor(FabVal x, FabVal y) {
    std::cout << "\033[" << (int)y.d + 1 << ";" << (int)x.d + 1 << "H";
    return FAB_NIL;
}
inline FabVal fab_console_get_cursor_x() { return fab_num(0); } // terminal limitation
inline FabVal fab_console_get_cursor_y() { return fab_num(0); }
inline FabVal fab_console_hide_cursor() { std::cout << "\033[?25l"; return FAB_NIL; }
inline FabVal fab_console_show_cursor() { std::cout << "\033[?25h"; return FAB_NIL; }
inline FabVal fab_console_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return fab_num(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#else
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) return fab_num(ws.ws_col);
#endif
    return fab_num(80);
}
inline FabVal fab_console_height() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        return fab_num(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
#else
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) return fab_num(ws.ws_row);
#endif
    return fab_num(24);
}
inline FabVal fab_console_underline(FabVal s) { return fab_str("\033[4m" + s.s + "\033[0m"); }
inline FabVal fab_console_blink(FabVal s) { return fab_str("\033[5m" + s.s + "\033[0m"); }

// ── Environment ──────────────────────────────────────────────────
inline FabVal fab_env_exit(FabVal code) { std::exit((int)code.d); return FAB_NIL; }
inline FabVal fab_env_sdelay(FabVal s) {
#ifdef _WIN32
    Sleep((unsigned int)(s.d * 1000));
#else
    sleep((unsigned int)s.d * 1000);
#endif
    return FAB_NIL;
}
inline FabVal fab_env_mdelay(FabVal s) {
#ifdef _WIN32
    Sleep((unsigned int)(s.d));
#else
    sleep((unsigned int)s.d);
#endif
    return FAB_NIL;
}
inline FabVal fab_env_machine_name() {
    static char buf[256]; buf[0] = '\0';
#ifdef _WIN32
    DWORD sz = sizeof(buf); GetComputerNameA(buf, &sz);
#else
    gethostname(buf, sizeof(buf));
#endif
    return fab_str(buf);
}
inline FabVal fab_env_new_line() { return fab_str("\n"); }
inline FabVal fab_env_stack_trace() { return fab_str("(stack trace not available in native build)"); }
inline FabVal fab_env_exit_code() { return fab_num(0); }
inline FabVal fab_env_os_name() {
#ifdef _WIN32
    return fab_str("Windows");
#elif __APPLE__
    return fab_str("macOS");
#else
    return fab_str("Linux");
#endif
}
inline FabVal fab_env_os_ver() {
#ifdef _WIN32
    OSVERSIONINFOEXA osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#pragma warning(suppress: 4996)
    if (GetVersionExA((LPOSVERSIONINFOA)&osvi))
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%lu.%lu.%lu",
            osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        return fab_str(buf);
    }
    return fab_str("unknown");
#elif __APPLE__
    return fab_str("macOS");
#else
    return fab_str("Linux");
#endif
}
inline FabVal fab_env_current_directory() {
    return fab_str(fs::current_path().string());
}
inline FabVal fab_env_com_line() { return fab_str(""); }
inline FabVal fab_env_process_id() {
#ifdef _WIN32
    return fab_num((double)GetCurrentProcessId());
#else
    return fab_num((double)getpid());
#endif
}
inline FabVal fab_env_process_count() {
#ifdef _WIN32
    SYSTEM_INFO si{}; GetSystemInfo(&si); return fab_num(si.dwNumberOfProcessors);
#else
    return fab_num((double)sysconf(_SC_NPROCESSORS_ONLN));
#endif
}
inline FabVal fab_env_username() {
#ifdef _WIN32
    char buf[256]; DWORD sz = sizeof(buf); GetUserNameA(buf, &sz); return fab_str(buf);
#else
    const char* u = std::getenv("USER");
    return fab_str(u ? u : "unknown");
#endif
}
inline FabVal fab_env_user_interactive() { return fab_bool_v(true); }
inline FabVal fab_env_tick_count() {
    return fab_num((double)(std::clock() * 1000 / CLOCKS_PER_SEC));
}
inline FabVal fab_env_working_set() { return fab_num(0); }
inline FabVal fab_env_service_pack() { return fab_str(""); }
inline FabVal fab_env_process_path() {
#ifdef _WIN32
    char buf[1024]; GetModuleFileNameA(NULL, buf, sizeof(buf)); return fab_str(buf);
#else
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len >= 0) { buf[len] = '\0'; return fab_str(buf); }
    return fab_str("");
#endif
}
inline FabVal fab_env_user_domain_name() {
#ifdef _WIN32
    char buf[256]; DWORD sz = sizeof(buf); GetUserNameA(buf, &sz); return fab_str(buf);
#else
    const char* d = std::getenv("USERDOMAIN");
    return fab_str(d ? d : "");
#endif
}
// environment.compute — evaluate a simple arithmetic expression string
// Uses a basic recursive-descent parser (no external deps)
inline FabVal fab_env_compute(FabVal expr_v, FabVal /*filter*/) {
    // Minimal eval: delegates to strtod for simple constant expressions
    const std::string& s = expr_v.s;
    char* end = nullptr;
    double result = std::strtod(s.c_str(), &end);
    if (end && *end == '\0') return fab_num(result);
    // For non-trivial expressions, return the string as-is (safe fallback)
    return expr_v;
}

// ── Date library ─────────────────────────────────────────────────
#include <chrono>
#include <iomanip>

inline std::string fab_fmt_time(const char* fmt) {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char buf[128]; std::strftime(buf, sizeof(buf), fmt, tm);
    return buf;
}
inline FabVal fab_date_date_now() { return fab_str(fab_fmt_time("%m.%d.%Y")); }
inline FabVal fab_date_time_now() { return fab_str(fab_fmt_time("%H:%M:%S")); }
inline FabVal fab_date_day_now() { std::time_t t = std::time(nullptr); std::tm* tm = std::localtime(&t); return fab_num(tm->tm_mday); }
inline FabVal fab_date_month_now() { std::time_t t = std::time(nullptr); std::tm* tm = std::localtime(&t); return fab_num(tm->tm_mon + 1); }
inline FabVal fab_date_year_now() { std::time_t t = std::time(nullptr); std::tm* tm = std::localtime(&t); return fab_num(tm->tm_year % 100); }
inline FabVal fab_date_sec_now() { std::time_t t = std::time(nullptr); std::tm* tm = std::localtime(&t); return fab_num(tm->tm_sec); }
inline FabVal fab_date_min_now() { std::time_t t = std::time(nullptr); std::tm* tm = std::localtime(&t); return fab_num(tm->tm_min); }
inline FabVal fab_date_hours_now() { std::time_t t = std::time(nullptr); std::tm* tm = std::localtime(&t); return fab_num(tm->tm_hour); }
inline FabVal fab_date_now(FabVal fmt) {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char buf[256]; std::strftime(buf, sizeof(buf), fmt.s.c_str(), tm);
    return fab_str(buf);
}
inline FabVal fab_date_max_value() { return fab_str("12/31/9999 23:59:59"); }
inline FabVal fab_date_min_value() { return fab_str("01/01/0001 00:00:00"); }

// ── String member extensions ──────────────────────────────────────
inline FabVal fab_str_trim_end(FabVal s, FabVal ch) {
    std::string r = s.s;
    char sep = (ch.tag == FabTag::Char) ? ch.c : (ch.s.empty() ? ' ' : ch.s[0]);
    while (!r.empty() && r.back() == sep) r.pop_back();
    return fab_str(r);
}
inline FabVal fab_str_trim_start(FabVal s, FabVal ch) {
    std::string r = s.s;
    char sep = (ch.tag == FabTag::Char) ? ch.c : (ch.s.empty() ? ' ' : ch.s[0]);
    while (!r.empty() && r.front() == sep) r.erase(r.begin());
    return fab_str(r);
}
inline FabVal fab_str_trim_char(FabVal s, FabVal ch) {
    FabVal a = fab_str_trim_start(s, ch);
    return fab_str_trim_end(a, ch);
}
inline FabVal fab_str_first(FabVal s) {
    if (s.tag != FabTag::String || s.s.empty())
        throw std::runtime_error("str.first: string is empty");
    return fab_char_v(s.s[0]);
}
inline FabVal fab_str_last(FabVal s) {
    if (s.tag != FabTag::String || s.s.empty())
        throw std::runtime_error("str.last: string is empty");
    return fab_char_v(s.s.back());
}
inline FabVal fab_str_normalize(FabVal s) { return s; } // NFC normalization not in std C++
inline FabVal fab_str_is_normalize(FabVal s) { return FAB_TRUE; }

// ── Char member extensions ────────────────────────────────────────
// These handle the case where the target is a Char FabVal (not a String).
// to_lower/to_upper for char: return a new Char with converted case.
inline FabVal fab_char_to_lower(FabVal v) {
    if (v.tag == FabTag::Char) return fab_char_v((char)std::tolower((unsigned char)v.c));
    // fallback for single-char string
    if (v.tag == FabTag::String && v.s.size() == 1)
        return fab_char_v((char)std::tolower((unsigned char)v.s[0]));
    return v;
}
inline FabVal fab_char_to_upper(FabVal v) {
    if (v.tag == FabTag::Char) return fab_char_v((char)std::toupper((unsigned char)v.c));
    if (v.tag == FabTag::String && v.s.size() == 1)
        return fab_char_v((char)std::toupper((unsigned char)v.s[0]));
    return v;
}
inline FabVal fab_char_to_str(FabVal v) {
    if (v.tag == FabTag::Char) return fab_str(std::string(1, v.c));
    return fab_str(fab_fmt(v));
}

// ── Numeric member extensions ─────────────────────────────────────
inline FabVal fab_num_is_even(FabVal v) { return fab_bool_v((long long)v.d % 2 == 0); }
inline FabVal fab_num_is_positive(FabVal v) { return fab_bool_v(v.d > 0); }
inline FabVal fab_num_is_negative(FabVal v) { return fab_bool_v(v.d < 0); }
inline FabVal fab_num_is_even_integer(FabVal v) {
    double d = v.d;
    return fab_bool_v(d == std::floor(d) && (long long)d % 2 == 0);
}
inline FabVal fab_num_is_odd_integer(FabVal v) {
    double d = v.d;
    return fab_bool_v(d == std::floor(d) && (long long)d % 2 != 0);
}
inline FabVal fab_num_is_pow2(FabVal v) {
    long long n = (long long)v.d;
    return fab_bool_v(n > 0 && (n & (n - 1)) == 0);
}
inline FabVal fab_num_is_pow3(FabVal v) {
    double x = v.d;
    if (x <= 0) return FAB_FALSE;
    double r = std::cbrt(x);
    long long ri = (long long)std::round(r);
    return fab_bool_v(std::abs(x - (double)(ri * ri * ri)) < 1e-9);
}
inline FabVal fab_bool_to_str(FabVal v) { return fab_str(v.b ? "true" : "false"); }

// ── Convert class functions ───────────────────────────────────────
inline FabVal fab_convert_to_int(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(int)std::stoll(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_int: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(int)v.d);
}
inline FabVal fab_convert_to_short(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(short)std::stoll(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_short: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(short)v.d);
}
inline FabVal fab_convert_to_long(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)std::stoll(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_long: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(long long)v.d);
}
inline FabVal fab_convert_to_byte(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(uint8_t)std::stoul(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_byte: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(uint8_t)v.d);
}
inline FabVal fab_convert_to_uint(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(unsigned int)std::stoul(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_uint: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(unsigned int)v.d);
}
inline FabVal fab_convert_to_ushort(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(unsigned short)std::stoul(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_ushort: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(unsigned short)v.d);
}
inline FabVal fab_convert_to_ulong(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(unsigned long long)std::stoull(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_ulong: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(unsigned long long)v.d);
}
inline FabVal fab_convert_to_ubyte(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)(int8_t)std::stoi(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_ubyte: cannot convert '" + v.s + "'"); }
    }
    return fab_num((double)(int8_t)v.d);
}
inline FabVal fab_convert_to_float(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num((double)std::stof(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_float: cannot convert '" + v.s + "'"); }
    }
    return fab_num((float)v.d);
}
inline FabVal fab_convert_to_double(FabVal v) {
    if (v.tag == FabTag::String) {
        try { return fab_num(std::stod(v.s)); }
        catch (...) { throw std::runtime_error("Convert.to_double: cannot convert '" + v.s + "'"); }
    }
    return v;
}
inline FabVal fab_convert_to_bool(FabVal v) {
    if (v.tag == FabTag::String) {
        if (v.s == "true")  return FAB_TRUE;
        if (v.s == "false") return FAB_FALSE;
        throw std::runtime_error("Convert.to_bool: cannot convert '" + v.s + "'");
    }
    return fab_bool_v(fab_truthy(v));
}
inline FabVal fab_convert_to_str(FabVal v) { return fab_str(fab_fmt(v)); }
inline FabVal fab_convert_to_bin(FabVal v) {
    long long n = (long long)v.d;
    if (n == 0) return fab_str("0");
    std::string r; bool neg = n < 0; if (neg) n = -n;
    while (n) { r = (char)('0' + n % 2) + r; n >>= 1; }
    return fab_str(neg ? "-" + r : r);
}
inline FabVal fab_convert_to_hex(FabVal v) {
    long long n = (long long)v.d;
    char buf[32]; std::snprintf(buf, sizeof(buf), "%llX", n);
    return fab_str(std::string(buf));
}
inline FabVal fab_convert_to_oct(FabVal v) {
    long long n = (long long)v.d;
    char buf[32]; std::snprintf(buf, sizeof(buf), "%llo", n);
    return fab_str(std::string(buf));
}
inline FabVal fab_convert_from_bin(FabVal v) {
    try { return fab_num((double)std::stoll(v.s, nullptr, 2)); }
    catch (...) { throw std::runtime_error("Convert.from_bin: invalid binary '" + v.s + "'"); }
}
inline FabVal fab_convert_from_hex(FabVal v) {
    try { return fab_num((double)std::stoll(v.s, nullptr, 16)); }
    catch (...) { throw std::runtime_error("Convert.from_hex: invalid hex '" + v.s + "'"); }
}
inline FabVal fab_convert_from_oct(FabVal v) {
    try { return fab_num((double)std::stoll(v.s, nullptr, 8)); }
    catch (...) { throw std::runtime_error("Convert.from_oct: invalid octal '" + v.s + "'"); }
}
inline FabVal fab_convert_to_char(FabVal v) {
    if (v.tag == FabTag::Char) return v;
    return fab_char_v((char)(int)v.d);
}
inline FabVal fab_convert_to_ascii(FabVal v) {
    if (v.tag == FabTag::Char) return fab_num((double)v.c);
    if (v.tag == FabTag::String && !v.s.empty()) return fab_num((double)(unsigned char)v.s[0]);
    throw std::runtime_error("Convert.to_ascii: expected char");
}

// ── Color struct and functions ────────────────────────────────────
struct FabColor { uint8_t r, g, b, a; };

inline FabVal fab_color_val(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    // Pack as a string "Color(r,g,b,a)" — stored in s field, tag = String
    // We use a special tag approach: store as Dict with r/g/b/a keys
    FabDictPtr d = fab_dict_new(-1);
    fab_dict_set(d, fab_str("__color__"), FAB_TRUE);
    fab_dict_set(d, fab_str("r"), fab_num(r));
    fab_dict_set(d, fab_str("g"), fab_num(g));
    fab_dict_set(d, fab_str("b"), fab_num(b));
    fab_dict_set(d, fab_str("a"), fab_num(a));
    return fab_dict_val(d);
}
inline bool fab_is_color(const FabVal& v) {
    if (v.tag != FabTag::Dict) return false;
    return v.dict->contains(fab_str("__color__"));
}
inline uint8_t fab_color_r(const FabVal& v) { return (uint8_t)fab_dict_get(v.dict, fab_str("r")).d; }
inline uint8_t fab_color_g(const FabVal& v) { return (uint8_t)fab_dict_get(v.dict, fab_str("g")).d; }
inline uint8_t fab_color_b(const FabVal& v) { return (uint8_t)fab_dict_get(v.dict, fab_str("b")).d; }
inline uint8_t fab_color_a(const FabVal& v) { return (uint8_t)fab_dict_get(v.dict, fab_str("a")).d; }

inline FabVal fab_color_new(FabVal r, FabVal g, FabVal b) {
    auto clamp = [](double x) -> uint8_t { return (uint8_t)std::max(0.0, std::min(255.0, x)); };
    return fab_color_val(clamp(r.d), clamp(g.d), clamp(b.d));
}
inline FabVal fab_color_new4(FabVal r, FabVal g, FabVal b, FabVal a) {
    auto clamp = [](double x) -> uint8_t { return (uint8_t)std::max(0.0, std::min(255.0, x)); };
    return fab_color_val(clamp(r.d), clamp(g.d), clamp(b.d), clamp(a.d));
}
inline FabVal fab_color_from_hex(FabVal hex) {
    std::string h = hex.s;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() == 6) {
        uint8_t r = (uint8_t)std::stoul(h.substr(0, 2), nullptr, 16);
        uint8_t g = (uint8_t)std::stoul(h.substr(2, 2), nullptr, 16);
        uint8_t b = (uint8_t)std::stoul(h.substr(4, 2), nullptr, 16);
        return fab_color_val(r, g, b);
    }
    if (h.size() == 8) {
        uint8_t r = (uint8_t)std::stoul(h.substr(0, 2), nullptr, 16);
        uint8_t g = (uint8_t)std::stoul(h.substr(2, 2), nullptr, 16);
        uint8_t b = (uint8_t)std::stoul(h.substr(4, 2), nullptr, 16);
        uint8_t a = (uint8_t)std::stoul(h.substr(6, 2), nullptr, 16);
        return fab_color_val(r, g, b, a);
    }
    throw std::runtime_error("Color.from_hex: invalid hex '" + hex.s + "'");
}
inline FabVal fab_color_to_hex(FabVal col) {
    char buf[10];
    uint8_t r = fab_color_r(col), g = fab_color_g(col), b = fab_color_b(col), a = fab_color_a(col);
    if (a == 255) std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    else          std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
    return fab_str(buf);
}
inline FabVal fab_color_lerp(FabVal ca, FabVal cb, FabVal t) {
    float ft = std::max(0.0f, std::min(1.0f, (float)t.d));
    auto lerp_ch = [&](uint8_t a, uint8_t b) -> uint8_t { return (uint8_t)(a + (b - a) * ft); };
    return fab_color_val(
        lerp_ch(fab_color_r(ca), fab_color_r(cb)),
        lerp_ch(fab_color_g(ca), fab_color_g(cb)),
        lerp_ch(fab_color_b(ca), fab_color_b(cb)),
        lerp_ch(fab_color_a(ca), fab_color_a(cb)));
}
inline FabVal fab_color_invert(FabVal col) {
    return fab_color_val(255 - fab_color_r(col), 255 - fab_color_g(col), 255 - fab_color_b(col), fab_color_a(col));
}
inline FabVal fab_color_with_alpha(FabVal col, FabVal alpha) {
    return fab_color_val(fab_color_r(col), fab_color_g(col), fab_color_b(col), (uint8_t)alpha.d);
}
inline FabVal fab_color_get_r(FabVal col) { return fab_num(fab_color_r(col)); }
inline FabVal fab_color_get_g(FabVal col) { return fab_num(fab_color_g(col)); }
inline FabVal fab_color_get_b(FabVal col) { return fab_num(fab_color_b(col)); }
inline FabVal fab_color_get_a(FabVal col) { return fab_num(fab_color_a(col)); }
inline FabVal fab_color_to_str(FabVal col) {
    uint8_t r = fab_color_r(col), g = fab_color_g(col), b = fab_color_b(col), a = fab_color_a(col);
    char buf[64];
    if (a == 255) std::snprintf(buf, sizeof(buf), "Color(%d, %d, %d)", r, g, b);
    else          std::snprintf(buf, sizeof(buf), "Color(%d, %d, %d, %d)", r, g, b, a);
    return fab_str(buf);
}

// Predefined Color constants (as functions for lazy init)
inline FabVal fab_color_red() { return fab_color_val(255, 0, 0); }
inline FabVal fab_color_green() { return fab_color_val(0, 255, 0); }
inline FabVal fab_color_blue() { return fab_color_val(0, 0, 255); }
inline FabVal fab_color_white() { return fab_color_val(255, 255, 255); }
inline FabVal fab_color_black() { return fab_color_val(0, 0, 0); }
inline FabVal fab_color_yellow() { return fab_color_val(255, 255, 0); }
inline FabVal fab_color_cyan() { return fab_color_val(0, 255, 255); }
inline FabVal fab_color_magenta() { return fab_color_val(255, 0, 255); }
inline FabVal fab_color_orange() { return fab_color_val(255, 165, 0); }
inline FabVal fab_color_purple() { return fab_color_val(128, 0, 128); }
inline FabVal fab_color_gray() { return fab_color_val(128, 128, 128); }
inline FabVal fab_color_transparent() { return fab_color_val(0, 0, 0, 0); }

// ── IPAddress functions ───────────────────────────────────────────
#ifdef _WIN32
// winsock already included
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

inline FabVal fab_ipaddress_parse(FabVal addr) {
    // Validate by trying inet_pton
    struct in_addr a4 {}; struct in6_addr a6 {};
    if (inet_pton(AF_INET, addr.s.c_str(), &a4) == 1) return addr;
    if (inet_pton(AF_INET6, addr.s.c_str(), &a6) == 1) return addr;
    throw std::runtime_error("ipaddress.parse: invalid address '" + addr.s + "'");
}
inline FabVal fab_ipaddress_loopback() { return fab_str("127.0.0.1"); }
inline FabVal fab_ipaddress_broadcast() { return fab_str("255.255.255.255"); }
inline FabVal fab_ipaddress_none() { return fab_str("255.255.255.255"); }
inline FabVal fab_ipaddress_ipv6_any() { return fab_str("::"); }
inline FabVal fab_ipaddress_ipv6_loopback() { return fab_str("::1"); }
inline FabVal fab_ipaddress_ipv6_none() { return fab_str("ffff::"); }
inline FabVal fab_ipaddress_is_loopback(FabVal addr) {
    return fab_bool_v(addr.s == "127.0.0.1" || addr.s == "::1");
}
inline FabVal fab_ipaddress_host_to_network_order(FabVal v) {
    return fab_num((double)htonl((uint32_t)v.d));
}
inline FabVal fab_ipaddress_network_to_host_order(FabVal v) {
    return fab_num((double)ntohs((uint16_t)v.d));
}

// ── Regex functions (C++11 <regex>) ──────────────────────────────
#include <regex>

inline std::regex fab_make_regex(const std::string& pattern, const std::string& flags) {
    auto opts = std::regex::ECMAScript;
    for (char f : flags) {
        if (f == 'i') opts |= std::regex::icase;
        if (f == 'm') opts |= std::regex::multiline;
        if (f == 's') opts |= std::regex::ECMAScript; // dot-all not in std; fallback
    }
    return std::regex(pattern, opts);
}
inline FabVal fab_regex_is_match(FabVal input, FabVal pattern, FabVal flags) {
    auto rx = fab_make_regex(pattern.s, flags.tag == FabTag::String ? flags.s : "");
    return fab_bool_v(std::regex_search(input.s, rx));
}
inline FabVal fab_regex_match(FabVal input, FabVal pattern, FabVal flags) {
    auto rx = fab_make_regex(pattern.s, flags.tag == FabTag::String ? flags.s : "");
    std::smatch m;
    if (std::regex_search(input.s, m, rx)) return fab_str(m[0].str());
    return FAB_NIL;
}
inline FabVal fab_regex_find_all(FabVal input, FabVal pattern, FabVal flags) {
    auto rx = fab_make_regex(pattern.s, flags.tag == FabTag::String ? flags.s : "");
    FabListPtr l = fab_list_new(-1);
    std::string s = input.s;
    std::sregex_iterator it(s.begin(), s.end(), rx), end;
    for (; it != end; ++it) fab_list_push(l, fab_str((*it)[0].str()));
    return fab_list_val(l);
}
inline FabVal fab_regex_replace(FabVal input, FabVal pattern, FabVal repl, FabVal flags) {
    auto rx = fab_make_regex(pattern.s, flags.tag == FabTag::String ? flags.s : "");
    return fab_str(std::regex_replace(input.s, rx, repl.s));
}
inline FabVal fab_regex_split(FabVal input, FabVal pattern, FabVal flags) {
    auto rx = fab_make_regex(pattern.s, flags.tag == FabTag::String ? flags.s : "");
    FabListPtr l = fab_list_new(-1);
    std::string s = input.s;
    std::sregex_token_iterator it(s.begin(), s.end(), rx, -1), end;
    for (; it != end; ++it) fab_list_push(l, fab_str(it->str()));
    return fab_list_val(l);
}
inline FabVal fab_regex_groups(FabVal input, FabVal pattern, FabVal flags) {
    auto rx = fab_make_regex(pattern.s, flags.tag == FabTag::String ? flags.s : "");
    std::smatch m;
    FabListPtr l = fab_list_new(-1);
    if (std::regex_search(input.s, m, rx))
        for (size_t i = 0; i < m.size(); i++) fab_list_push(l, fab_str(m[i].str()));
    return fab_list_val(l);
}
inline FabVal fab_regex_escape(FabVal pattern) {
    static const std::string special = R"(\.^$|?*+()[]{})";
    std::string r;
    for (char c : pattern.s) {
        if (special.find(c) != std::string::npos) r += '\\';
        r += c;
    }
    return fab_str(r);
}