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
        Parser/AST/ValueExpression.h
        Parser/AST/BinExpression.h
        Parser/AST/UnaryExpression.h
        Parser/AST/VariableExpression.h
        Parser/AST/Statement.h
        Parser/AST/AssigementStatement.h
        Parser/AST/IOStatement.h
        libs/Value.h
        libs/Environment.h
        libs/StringValue.h
        libs/NumberValue.h
        libs/BooleanValue.h
        Parser/AST/ConditionalExpression.h
        Parser/AST/IfStatement.h
        Parser/AST/LoopStatement.h
        Parser/AST/BreakStatement.h
)
