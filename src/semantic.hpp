#pragma once

#include "ast.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace Semantic {

// Учебная промежуточная версия семантического анализатора.
// Здесь оставлены базовые проверки, а часть более сложных конструкций
// помечена как TODO и пока считается неподдержанной.

struct SemanticError {
    std::string filename {};
    std::string message {};
    Lexer::SourceLocation location {};
};

std::string format_error(const SemanticError& error);

enum class SemanticTypeKind {
    Builtin,
    Array,
    Struct,
};

struct SemanticType {
    SemanticTypeKind kind = SemanticTypeKind::Builtin;
    std::string name {};
    std::string element_type_name {};
    std::size_t array_size = 0;
};

struct FunctionInfo {
    std::string full_name {};
    std::vector<SemanticType> parameter_types;
    SemanticType return_type {};
    bool is_builtin = false;
};

struct StructFieldInfo {
    std::string name {};
    SemanticType type {};
};

struct StructInfo {
    std::string full_name {};
    std::vector<StructFieldInfo> fields;
};

struct VariableInfo {
    SemanticType type {};
    bool is_mutable = false;
};

struct SemanticResult {
    const AST::Program* program = nullptr;

    std::unordered_map<const AST::Expr*, SemanticType> expr_types;
    std::unordered_map<const AST::Expr*, std::string> resolved_functions;
    std::unordered_map<const AST::FunctionDecl*, FunctionInfo> functions;
    std::unordered_map<const AST::StructDecl*, StructInfo> structs;
    std::unordered_map<const AST::TypeAliasDecl*, SemanticType> aliases;
    std::unordered_map<const AST::VariableDeclStmt*, VariableInfo> variables;
};

std::expected<SemanticResult, SemanticError> analyze_program(
    const AST::Program& program, std::string filename = "<memory>");

} 