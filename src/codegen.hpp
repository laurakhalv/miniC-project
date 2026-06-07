#pragma once

#include "ast.hpp"
#include "semantic.hpp"

#include <expected>
#include <string>

namespace Codegen {

struct CodegenError {
    std::string filename {};
    std::string message {};
    Lexer::SourceLocation location {};
};

std::string format_error(const CodegenError& error);

std::expected<std::string, CodegenError> generate_program(
    const AST::Program& program,
    const Semantic::SemanticResult& semantic_result,
    std::string filename = "<memory>");

}  
