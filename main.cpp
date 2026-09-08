#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>

#include "Parser/Lexer.h"
#include "Parser/Parser.h"

#include "libs/Environment.h"

#include "CodeGen/Compile.h"

#include <filesystem>

static bool is_compiled = false;

void info()
{
    std::cout << "FAB# Interpreter\tv0.3" << std::endl
              << std::endl
              << "Start arguments:" << std::endl
              << "--help/-h - show this messege" << std::endl
              << std::endl
              << "How run the script?" << std::endl
              << "interpreter_path script_path.fab" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
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
                std::string name = std::filesystem::directory_entry(argv[1]).path().filename().string();

                compile(expression, name);

                std::string cmd = "\"\"C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/bin/Hostx64/x64/link.exe\" "
                                "" + name + ".obj /out:" + name.substr(0, name.find('.')) + ".exe /subsystem:console /defaultlib:libcmt "
                                "/LIBPATH:\"C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/lib/x64\" "
                                "/LIBPATH:\"C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64\" "
                                "/LIBPATH:\"C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64\"\"";

                system(cmd.c_str());

                cmd = "\".\\" + name.substr(0, name.find('.')) + ".exe\"";

                system(cmd.c_str());
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