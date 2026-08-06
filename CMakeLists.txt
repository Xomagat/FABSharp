cmake_minimum_required(VERSION 4.3)
project(FABSharp)

set(CMAKE_CXX_STANDARD 20)

add_executable(FABSharp main.cpp
        Parser/Lexer.cpp
        Parser/Lexer.h
        Parser/Token.cpp
        Parser/Token.h
        Parser/TokenType.h)
