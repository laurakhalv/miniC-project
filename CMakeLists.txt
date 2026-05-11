cmake_minimum_required(VERSION 3.20)
project(MiniC LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(minic
    src/lexer.cpp
    src/token.cpp
    src/ast.cpp       
    src/parser.cpp    
    src/main.cpp
)
target_include_directories(minic PRIVATE src)