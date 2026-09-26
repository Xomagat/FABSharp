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

#include "libs/Environment.h"

#include "CodeGen/Compile.h"

#ifdef _WIN32
#include <windows.h>
std::filesystem::path get_executable_dir()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}
#else
#include <unistd.h>
std::filesystem::path get_executable_dir()
{
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    return std::filesystem::path(buffer).parent_path();
}
#endif

// App Settings
#define VERSION "0.1"

// System vars
static bool is_compiled = false;

// Functions
void info()
{
    std::cout << "FAB# Interpreter(& Compiler)\tv" << VERSION << std::endl
              << std::endl
              << "Start arguments:" << std::endl
              << "--help/-h\t\t- show this message" << std::endl
              << "--compile/-cmp\t\t- change mode to compile" << std::endl
              << std::endl
              << "How run the script?" << std::endl
              << "interpreter_path script_path.fab" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        if (std::strcmp(argv[1], "--version") == 0 || std::strcmp(argv[1], "-v") == 0)
        {
            std::cout << "FAB# Interpreter(& Compiler)\tv" << VERSION << std::endl;
            return 0;
        }
        if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)
        {
            info();
            return 0;
        }
        if (std::strcmp(argv[2], "--compile") == 0 || std::strcmp(argv[2], "-cmp") == 0)
        {
            is_compiled = true;
        }

        std::ifstream file(argv[1]);
        std::stringstream buffer;

        if (!file.is_open())
        {
            std::cout << "Error opening file!" << std::endl;

            return 1;
        }

        buffer << file.rdbuf();

        std::string input = buffer.str();

        try
        {
            auto tokens = Lexer(input).tokenize();
            auto expression = Parser(tokens).parse();

            if (is_compiled)
            {
                auto name = std::filesystem::directory_entry(argv[1]);

                compile(expression, name.path().string());

                auto toolsPath = get_executable_dir() / "tools";

                std::string objFile = name.path().string() + ".obj";
                std::string exeFile = name.path().string().substr(0, name.path().string().rfind('.')) + ".exe";

                std::string cmd = "\"\"" + (toolsPath / "lld-link.exe").string() + "\" "
                   "\"" + (toolsPath / "crt" / "crt2.o").string() + "\" "
                   "\"" + (toolsPath / "crt" / "crtbegin.o").string() + "\" "
                   "\"" + objFile + "\" "
                   "\"" + (toolsPath / "crt" / "crtend.o").string() + "\" "
                   "/out:\"" + exeFile + "\""
                   " /subsystem:console /entry:mainCRTStartup "
                   "/LIBPATH:\"" + (toolsPath / "libs").string() + "\" "
                   "libmingw32.a libmingwex.a libmsvcrt.a libkernel32.a libgcc.a\""; // <-- уберите последнюю \"

                system(cmd.c_str());

                cmd = "\".\\" + name.path().string().substr(0, name.path().string().rfind('.')) + ".exe\"";

                int exit_code = system(cmd.c_str());

                std::filesystem::remove(name.path().string() + ".obj");

                return exit_code;
            }
            else
            {
                Environment global;

                for (auto& expr : expression)
                {
                    expr->execute(global);
                }
            }
        }
        catch (std::exception& e)
        {
            std::cout << "Runtime error: " << e.what() << std::endl;
        }

        return 0;
    }
    else
    {
        std::cout << "Invalid argument!" << std::endl;

        return 2;
    }
}