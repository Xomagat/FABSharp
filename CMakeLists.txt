cmake_minimum_required(VERSION 4.3)
project(fab)

set(CMAKE_CXX_STANDARD 20)

set(LLVM_DIR "llvm/lib/cmake/llvm")
find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(fab main.cpp
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
        Parser/AST/FunctionalExpression.h
        libs/Function.h
        libs/Functions.h
        Parser/AST/FunctionStatement.h
        Parser/AST/ReturnStatement.h
        CodeGen/CodegenContext.h
        CodeGen/Compile.cpp
        CodeGen/Compile.h
)

llvm_map_components_to_libnames(llvm_libs core support target x86asmparser x86codegen)
target_link_libraries(fab ${llvm_libs})