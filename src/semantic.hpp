#pragma once

#include "ast.hpp"

#include <expected>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace Semantic {

    //Структура ошибки семантики
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

//тип в публичном виде передаётся кодогену через SemanticResult
struct SemanticType {
    SemanticTypeKind kind = SemanticTypeKind::Builtin;
    std::string name {};
    std::string element_type_name {};
    std::size_t array_size = 0;
};

//Информация о функциях, полях/методах структур, переменных. Всё в публичном виде для кодогена
struct FunctionInfo {
    std::string full_name {};
    std::vector<SemanticType> parameter_types;
    SemanticType return_type {};
    bool is_builtin = false;
    bool is_method = false;
    std::string owner_struct_name {};
};

struct StructFieldInfo {
    std::string name {};
    SemanticType type {};
    bool is_private = false;
};

struct StructMethodInfo {
    std::string name {};
    std::string full_name {};
    std::vector<SemanticType> parameter_types;
    SemanticType return_type {};
    bool is_private = false;
};

struct StructInfo {
    std::string full_name {};
    std::vector<StructFieldInfo> fields;
    std::vector<StructMethodInfo> methods;
};

struct VariableInfo {
    SemanticType type {};
    bool is_mutable = false;
};

//Главный результат всей семантики — набор таблиц с аннотациями
struct SemanticResult {
    const AST::Program* program = nullptr;

    std::unordered_map<const AST::Expr*, SemanticType> expr_types;
    std::unordered_map<const AST::Expr*, std::string> resolved_functions;
    std::unordered_map<const AST::CallExpr*, std::string> resolved_calls;
    std::unordered_map<const AST::CallExpr*, std::vector<const AST::Expr*>> resolved_call_arguments;
    std::unordered_map<const AST::UnaryExpr*, std::string> resolved_unary_operator_calls;
    std::unordered_map<const AST::UnaryExpr*, std::vector<const AST::Expr*>>
        resolved_unary_operator_arguments;
    std::unordered_map<const AST::BinaryExpr*, std::string> resolved_binary_operator_calls;
    std::unordered_map<const AST::BinaryExpr*, std::vector<const AST::Expr*>>
        resolved_binary_operator_arguments;
    std::unordered_map<const AST::FunctionDecl*, FunctionInfo> functions;
    std::unordered_map<const AST::StructDecl*, StructInfo> structs;
    std::unordered_map<const AST::TypeAliasDecl*, SemanticType> aliases;
    std::unordered_map<const AST::VariableDeclStmt*, VariableInfo> variables;
};

std::expected<SemanticResult, SemanticError> analyze_program(
    const AST::Program& program, std::string filename = "<memory>");

} 