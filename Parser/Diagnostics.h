//
// Created by Xomagat on 06.11.2026.
//

#pragma once
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

bool diag_color();

struct SrcLoc
{
    int file = -1, line = 0, col = 0, len = 0;
    bool valid() const { return file >= 0 && line > 0; }
};

class FabError : public std::runtime_error
{
public:
    SrcLoc loc;
    FabError(const std::string& msg, SrcLoc l = {}) : std::runtime_error(msg), loc(l) {}
};

// ---------- source code registry ----------
struct SourceFile { std::string path, text; std::vector<size_t> starts; };
inline std::vector<SourceFile> g_sources;
inline std::vector<FabError>   g_diags;

inline int add_source(const std::string& path, const std::string& text)
{
    SourceFile f{path, text, {0}};
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n') f.starts.push_back(i + 1);
    g_sources.push_back(std::move(f));
    return (int)g_sources.size() - 1;
}

inline std::string source_line(int file, int line)
{
    auto& f = g_sources[file];
    if (file < 0 || file >= (int)g_sources.size()) return "";
    size_t b = f.starts[line - 1];
    size_t e = f.text.find('\n', b);
    std::string s = f.text.substr(b, e == std::string::npos ? std::string::npos : e - b);
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

template<class F> decltype(auto) guard(const SrcLoc& loc, F&& f)
{
    try { return f(); }
    catch (const FabError&) { throw; }
    catch (const std::runtime_error& e)
    {
        if (!loc.valid()) throw;
        throw FabError(e.what(), loc);
    }
}

// ---------- print ----------
struct Shown { std::string text; int from = 0, to = 0; };

inline Shown render_line(const std::string& src, int col, int len)
{
    Shown r; int w = 0;
    size_t from_b = (size_t)std::max(col - 1, 0);
    size_t to_b   = from_b + (size_t)std::max(len, 1);
    bool got_from = false, got_to = false;

    for (size_t i = 0; i <= src.size(); ++i)
    {
        if (i == from_b) { r.from = w; got_from = true; }
        if (i == to_b)   { r.to   = w; got_to   = true; }
        if (i == src.size()) break;

        unsigned char c = src[i];
        if (c == '\t') { r.text += "    "; w += 4; }
        else { r.text += (char)c; if ((c & 0xC0) != 0x80) ++w; }
    }
    if (!got_from) r.from = w;
    if (!got_to)   r.to = w;
    if (r.to <= r.from) r.to = r.from + 1;
    return r;
}

inline void print_diag(const FabError& e)
{
    std::ostream& os = std::cout;
    bool c = diag_color();
    auto K = [&](const char* code) { return c ? code : ""; };

    const char* RST  = K("\033[0m");
    const char* RED  = K("\033[1;31m");
    const char* MSG  = K("\033[1;97m");
    const char* DIM  = K("\033[90m");
    const char* CODE = K("\033[97m");
    const char* CTX  = K("\033[90m");

    const SrcLoc& l = e.loc;

    os << "\n " << RED << "error" << RST << "  " << MSG << e.what() << RST << "\n";

    if (!l.valid() || l.file >= (int)g_sources.size()) return;

    std::string cur  = source_line(l.file, l.line);
    std::string prev = l.line > 1 ? source_line(l.file, l.line - 1) : "";
    bool has_prev = prev.find_first_not_of(" \t") != std::string::npos;

    int w = (int)std::to_string(l.line).size();
    std::string pad(w, ' ');
    auto num = [&](int n) {
        std::string s = std::to_string(n);
        return std::string(w - s.size(), ' ') + s;
    };

    os << DIM << " " << pad << " ╭─ " << RST << l.line << ":" << l.col << "\n";

    if (has_prev)
        os << DIM << " " << num(l.line - 1) << " │ " << CTX << render_line(prev, 1, 1).text << RST << "\n";

    Shown s = render_line(cur, l.col, l.len);
    os << DIM << " " << num(l.line) << " │ " << CODE << s.text << RST << "\n";
    os << DIM << " " << pad << " │ " << RST
       << std::string(s.from, ' ') << RED << std::string(s.to - s.from, '^') << RST << "\n";
    os << DIM << " " << pad << " ╰─" << RST << "\n";
}


inline void print_all_diags()
{
    std::vector<FabError> v = g_diags;
    std::stable_sort(v.begin(), v.end(), [](const FabError& a, const FabError& b) {
        bool av = a.loc.valid(), bv = b.loc.valid();
        if (av != bv) return av;
        if (a.loc.file != b.loc.file) return a.loc.file < b.loc.file;
        if (a.loc.line != b.loc.line) return a.loc.line < b.loc.line;
        return a.loc.col < b.loc.col;
    });

    std::vector<FabError> uniq;
    for (auto& e : v)
    {
        bool dup = false;
        for (auto& u : uniq)
            if (u.loc.file == e.loc.file && u.loc.line == e.loc.line &&
                u.loc.col == e.loc.col && std::string(u.what()) == e.what()) { dup = true; break; }
        if (!dup) uniq.push_back(e);
    }

    bool c = diag_color();
    const char* RED = c ? "\033[1;31m" : "";
    const char* GRY = c ? "\033[90m"   : "";
    const char* RST = c ? "\033[0m"    : "";

    int last_file = -2;
    size_t shown = 0;
    for (auto& e : uniq)
    {
        if (shown++ == 20) break;

        if (e.loc.valid() && e.loc.file < (int)g_sources.size() && e.loc.file != last_file)
        {
            last_file = e.loc.file;
            std::cout << "\n" << RED << "—— " << GRY << "FAB#" << RED << " / " << RST
                      << g_sources[last_file].path << " " << RED << "——————————————" << RST << "\n";
        }
        print_diag(e);
    }

    if (uniq.size() > 20)
        std::cout << "\n" << GRY << "… and " << (uniq.size() - 20) << " more" << RST << "\n";

    std::cout << "\n" << RED << "✖ " << uniq.size() << (uniq.size() == 1 ? " error" : " errors") << RST << "\n";
}