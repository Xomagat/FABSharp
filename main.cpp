#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include "Parser/Lexer.h"
#include "Parser/Parser.h"

int main()
{
    std::ifstream file("D:/Projects/JustProgrammigProjects/C++ project/FABSharp/test.fab");
    std::stringstream buffer;

    if (!file.is_open())
    {
        std::cout << "Error opening file" << std::endl;
    }

    buffer << file.rdbuf();

    std::string input = buffer.str();

    auto tokens = Lexer(input).tokenize();

    auto expression = Parser(tokens).parse();

    for (auto& expr : expression)
    {
        expr->execute();
    }
}