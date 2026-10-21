#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "Parser/Lexer.h"
#include "Parser/Parser.h"

#include "CodeGen/Compile.h"

#ifdef _WIN32
#include <windows.h>
inline std::filesystem::path get_executable_dir()
{
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::string(buffer, len)).parent_path();
}
#else
#include <unistd.h>
inline std::filesystem::path get_executable_dir()
{
    char buffer[4096];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len < 0) return std::filesystem::current_path();
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}
#endif

// App Settings
#define VERSION "0.3"

struct CliOptions
{
    std::string input;
    std::string output;
    bool compile_only = false;
    bool emit_asm     = false;
    bool emit_llvm    = false;
    bool check_only   = false;
    bool run          = false;
    bool keep_obj     = false;
    bool verbose      = false;
    CompileOptions co;
};

void info()
{
    std::cout
        << "FAB# Compiler\tv" << VERSION << "\n\n"
        << "Usage: fab [options] script.fab\n\n"
        << "Options:\n"
        << "  -h, --help        show this message\n"
        << "  -v, --version     show version\n"
        << "  -o <file>         output file name\n"
        << "  -O0 -O1 -O2 -O3   optimization level (default -O2)\n"
        << "  -Os, -Oz          optimize for size\n"
        << "  -c                compile only, do not link (.o)\n"
        << "  -S                emit assembly (.s)\n"
        << "  --emit-llvm       emit LLVM IR (.ll)\n"
        << "  --check           parse only, no code generation\n"
        << "  -r, --run         run the program after build\n"
        << "  --keep-obj        keep the object file after linking\n"
        << "  --cpu=<name>      target CPU (default x86-64-v3)\n"
        << "  -V, --verbose     print build stages\n";
}

int parse_args(int argc, char** argv, CliOptions& o)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];

        if      (a == "-h" || a == "--help")    { info(); return 0; }
        else if (a == "-v" || a == "--version") { std::cout << "FAB# v" << VERSION << std::endl; return 0; }
        else if (a == "-o")
        {
            if (i + 1 >= argc) { std::cout << "Option -o needs a file name!" << std::endl; return 2; }
            o.output = argv[++i];
        }
        else if (a == "-O0") o.co.opt_level = 0;
        else if (a == "-O1") o.co.opt_level = 1;
        else if (a == "-O2") o.co.opt_level = 2;
        else if (a == "-O3") o.co.opt_level = 3;
        else if (a == "-Os") o.co.size_opt = 's';
        else if (a == "-Oz") o.co.size_opt = 'z';
        else if (a == "-c")  o.compile_only = true;
        else if (a == "-S")  o.emit_asm = true;
        else if (a == "--emit-llvm") o.emit_llvm = true;
        else if (a == "--check")     o.check_only = true;
        else if (a == "-r" || a == "--run") o.run = true;
        else if (a == "--keep-obj")  o.keep_obj = true;
        else if (a == "-V" || a == "--verbose") o.verbose = o.co.verbose = true;
        else if (a.starts_with("--cpu=")) o.co.cpu = a.substr(6);
        else if (a.starts_with("-") && a.size() > 1)
        {
            std::cout << "Unknown option: " << a << " (try --help)" << std::endl;
            return 2;
        }
        else if (o.input.empty()) o.input = a;
        else { std::cout << "Only one input file is supported!" << std::endl; return 2; }
    }

    if (o.input.empty()) { std::cout << "Invalid argument!" << std::endl; return 2; }
    return -1;
}

int main(int argc, char** argv)
{
    CliOptions o;
    int rc = parse_args(argc, argv, o);
    if (rc >= 0) return rc;

    std::ifstream file(o.input);
    if (!file.is_open())
    {
        std::cout << "Error opening file!" << std::endl;
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string input = buffer.str();

    try
    {
        namespace fs = std::filesystem;
        fs::path in_path = o.input;
        fs::path stem    = in_path;  stem.replace_extension("");

        if (o.verbose) std::cout << "[fab] lexing + parsing " << in_path << "\n";
        auto tokens = Lexer(input).tokenize();
        auto statements = Parser(tokens, in_path.parent_path(), get_executable_dir()).parse();

        if (o.check_only)
        {
            std::cout << "OK: no syntax errors" << std::endl;
            return 0;
        }

#ifdef _WIN32
        const std::string obj_ext = ".obj", exe_ext = ".exe";
#else
        const std::string obj_ext = ".o",   exe_ext = "";
#endif
        std::string objFile = in_path.string() + obj_ext;
        std::string exeFile = o.output.empty() ? stem.string() + exe_ext : o.output;

        bool only_text = (o.emit_asm || o.emit_llvm) && !o.compile_only;

        if (o.emit_llvm) o.co.ir_path  = (only_text && !o.emit_asm && !o.output.empty()) ? o.output : stem.string() + ".ll";
        if (o.emit_asm)  o.co.asm_path = (only_text && !o.emit_llvm && !o.output.empty()) ? o.output : stem.string() + ".s";
        o.co.skip_obj = only_text;

        if (o.compile_only && !o.output.empty()) objFile = o.output;

        if (o.verbose) std::cout << "[fab] generating code (-O" << o.co.opt_level << ")\n";
        compile(statements, objFile, o.co);

        if (only_text)
        {
            std::cout << "Success!" << std::endl;
            return 0;
        }
        if (o.compile_only)
        {
            std::cout << "Success! Object file: " << objFile << std::endl;
            return 0;
        }

        auto exe = get_executable_dir();
        auto tl  = exe / "tools";
        auto q   = [](const fs::path& p) { return "\"" + p.string() + "\""; };

#ifndef _WIN32
        auto glib = tl / "glibc";

        std::vector<std::string> libm_files;
        for (auto& e : fs::directory_iterator(glib))
        {
            auto n = e.path().filename().string();
            if (n.starts_with("libm") && n.ends_with(".a"))
                libm_files.push_back(q(e.path()));
        }
        std::sort(libm_files.begin(), libm_files.end());
        if (libm_files.empty())
            throw std::runtime_error("libm*.a not found in tools/glibc! Install glibc-static.");

        std::string libm_args;
        for (auto& f : libm_files) libm_args += f + " ";

        std::string cmd =
            q(tl / "ld.lld") + " -static "
            + q(glib / "crt1.o") + " " + q(glib / "crti.o") + " "
            + q(objFile) + " "
            + "--start-group "
            + q(exe / "lib" / "libfabstd.a") + " "
            + libm_args
            + q(glib / "libc.a") + " " + q(glib / "libgcc.a") + " " + q(glib / "libgcc_eh.a") + " "
            + "--end-group "
            + q(glib / "crtn.o") + " "
            + "-o " + q(exeFile);
#else
        std::string cmd = "\"\"" + (tl / "lld-link.exe").string() + "\" "
                          "\"" + (tl / "crt" / "crt2.o").string() + "\" "
                          "\"" + (tl / "crt" / "crtbegin.o").string() + "\" "
                          "\"" + objFile + "\" "
                          "\"" + (tl / "crt" / "crtend.o").string() + "\" "
                          "/out:\"" + exeFile + "\""
                          " /subsystem:console /entry:mainCRTStartup "
                          "/LIBPATH:\"" + (tl / "libs").string() + "\" "
                          "libmingw32.a libmingwex.a libmsvcrt.a libkernel32.a libgcc.a "
                          "/LIBPATH:\"" + (exe / "lib").string() + "\" "
                          "fabstd.lib\"";
#endif

        if (o.verbose) std::cout << "[fab] linking: " << cmd << "\n";
        int code = system(cmd.c_str());
        if (code != 0)
            throw std::runtime_error("Link failed! Exit code: " + std::to_string(code));

        if (!o.keep_obj) fs::remove(objFile);

        std::cout << "Success! " << exeFile << std::endl;

        if (o.run)
        {
            std::string run_cmd = "\"" + fs::absolute(exeFile).string() + "\"";
            if (o.verbose) std::cout << "[fab] running " << run_cmd << "\n";
            int r = system(run_cmd.c_str());
            std::cout << "\n[fab] program exited with code " << r << std::endl;
            return 0;
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Compile error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}