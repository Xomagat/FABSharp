#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "Parser/Lexer.h"
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

    std::vector<Token> tokens = Lexer(input).tokenize();

    for (Token& token : tokens)
    {
        std::cout << token.get_type() << ' ' << token.get_text() << std::endl;
    }
}