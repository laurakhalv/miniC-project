cmake_minimum_required(VERSION 3.20)

project(MiniC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minic
    src/ast.cpp
    src/codegen.cpp
    src/lexer.cpp
    src/main.cpp
    src/parser.cpp
    src/semantic.cpp
    src/token.cpp
)

target_include_directories(minic PRIVATE src)
target_compile_definitions(minic PRIVATE
    MINIC_RUNTIME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/runtime/minic_runtime.s"
)

add_custom_target(run
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/minic
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/semantic_ok.mc
    DEPENDS minic
    USES_TERMINAL
)

if(APPLE)
    set(MINIC_DEBUGGER lldb)
    set(MINIC_DEBUGGER_ARGS --)
else()
    set(MINIC_DEBUGGER gdb)
    set(MINIC_DEBUGGER_ARGS --args)
endif()

add_custom_target(debug
    COMMAND ${MINIC_DEBUGGER} ${MINIC_DEBUGGER_ARGS}
            ${CMAKE_CURRENT_BINARY_DIR}/minic
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/semantic_ok.mc
    DEPENDS minic
    USES_TERMINAL
)