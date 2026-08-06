#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Parser/Token.h"

int main()
{
    std::ifstream file("D:/Projects/JustProgrammigProjects/C++ project/FABSharp/test.fab");

    if (!file.is_open())
    {
        std::cout << "Error opening file" << std::endl;
    }

    std::string input;

    while (std::getline(file, input)){}

    auto tokens = Lexer(input).tokenize();

    Parser parser(tokens);
    auto expression = parser.parse();

    for (auto& expr : expression)
    {
        std::cout << expr->to_str() << " = " << expr->eval() << std::endl;
    }
}