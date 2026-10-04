cmake_minimum_required(VERSION 3.31.11)
project(fab)

set(CMAKE_CXX_STANDARD 20)

if (WIN32)
    set(LLVM_DIR "llvm/lib/cmake/llvm")
endif()

find_package(LLVM REQUIRED CONFIG)

if (NOT WIN32)
    find_library(ZSTD_STATIC NAMES libzstd.a REQUIRED)
endif()

foreach(t zstd::libzstd_shared zstd::libzstd_static)
    if (TARGET ${t})
        set_target_properties(${t} PROPERTIES
                IMPORTED_LOCATION "${ZSTD_STATIC}"
                IMPORTED_LOCATION_NONE "${ZSTD_STATIC}"
                IMPORTED_LOCATION_DEBUG "${ZSTD_STATIC}"
                IMPORTED_LOCATION_RELEASE "${ZSTD_STATIC}")
    endif()
endforeach()

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
        Parser/AST/ConditionalExpression.h
        Parser/AST/IfStatement.h
        Parser/AST/LoopStatement.h
        Parser/AST/BreakStatement.h
        Parser/AST/ContinueStatement.h
        Parser/AST/FunctionalExpression.h
        Parser/AST/FunctionStatement.h
        Parser/AST/ReturnStatement.h
        CodeGen/CodegenContext.h
        CodeGen/Compile.cpp
        CodeGen/Compile.h
        libs/STDLibInfo.h
        Parser/AST/UseStatement.h
)

if (NOT WIN32)
    foreach(lib libc.a libm.a libgcc.a libgcc_eh.a crt1.o crti.o crtn.o)
        execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=${lib}
                OUTPUT_VARIABLE _p OUTPUT_STRIP_TRAILING_WHITESPACE)

        if (NOT EXISTS "${_p}")
            message(FATAL_ERROR "${lib} not found. Install glibc-static.")
        endif()

        # libm.a и libc.a могут быть текстовыми скриптами GROUP(...)
        file(READ "${_p}" _head LIMIT 64 HEX)
        if (_head MATCHES "^2169726368") # "!<arch" = настоящий архив
            set(_files "${_p}")
        else()
            file(READ "${_p}" _txt)
            string(REGEX MATCH "GROUP *\\(([^)]*)\\)" _m "${_txt}")
            string(REGEX MATCHALL "/[^ ]+\\.a" _files "${CMAKE_MATCH_1}")
        endif()

        foreach(f ${_files})
            get_filename_component(_n "${f}" NAME)
            add_custom_command(TARGET fab POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${f}"
                    "$<TARGET_FILE_DIR:fab>/tools/glibc/${_n}")
        endforeach()
    endforeach()
endif()

add_library(fabstd STATIC
        lib/math.cpp
)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(fab PRIVATE /utf-8)
endif()

set_target_properties(fabstd PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:fab>/lib")

llvm_map_components_to_libnames(llvm_libs core support target x86asmparser x86codegen)
target_link_libraries(fab ${llvm_libs})

if (MINGW)
    target_link_options(fab PRIVATE -static-libgcc -static-libstdc++)
elseif (UNIX AND NOT APPLE)
    target_link_options(fab PRIVATE -static -s)
endif()

add_dependencies(fab fabstd)