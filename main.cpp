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

            Environment global;

            for (auto& expr : expression)
            {
                expr->execute(global);
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