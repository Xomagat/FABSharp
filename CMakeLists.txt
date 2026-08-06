cmake_minimum_required(VERSION 4.3)
project(FABSharp)

set(CMAKE_CXX_STANDARD 20)

add_executable(FABSharp main.cpp
        Parser/Lexer.cpp
        Parser/Lexer.h
        Parser/Token.cpp
        Parser/Token.h
        Parser/TokenType.h
        Parser/Parser.cpp
        Parser/Parser.h
        Parser/AST/Expression.h
        Parser/AST/NumberExpression.h
        Parser/AST/BinExpression.h
        Parser/AST/UnaryExpression.h)
