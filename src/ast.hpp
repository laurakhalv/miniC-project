#pragma once

#include "token.hpp"

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AST {

using SourceRange = Lexer::SourceRange;
using TokenType = Lexer::TokenType;

struct Node {
    explicit Node(SourceRange source_range);
    virtual ~Node() = default;

    SourceRange range;

    virtual void dump(std::ostream& out, int indent) const = 0;
};

enum class Mutability {
    Mutable,
    Immutable,
};

struct Decl : Node {
    using Node::Node;
};

struct Stmt : Node {
    using Node::Node;
};

struct Expr : Node {
    using Node::Node;
};

struct TypeSyntax final : Node {
    std::vector<std::string> name_parts;
    std::optional<std::string> array_size;

    TypeSyntax(SourceRange range, std::vector<std::string> parts,
               std::optional<std::string> size = std::nullopt);

    void dump(std::ostream& out, int indent) const override;
};

struct Parameter {
    std::unique_ptr<TypeSyntax> type;
    std::string name;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

struct FieldDecl {
    std::unique_ptr<TypeSyntax> type;
    std::string name;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

struct FieldInitializer {
    std::string name;
    std::unique_ptr<Expr> value;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

struct Program {
    std::vector<std::unique_ptr<Decl>> declarations;

    void dump(std::ostream& out) const;
};

struct BlockStmt final : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    BlockStmt(SourceRange range, std::vector<std::unique_ptr<Stmt>> body_statements);

    void dump(std::ostream& out, int indent) const override;
};

struct FunctionDecl final : Decl {
    std::string name;
    std::vector<Parameter> parameters;
    std::unique_ptr<TypeSyntax> return_type;
    std::unique_ptr<BlockStmt> body;

    FunctionDecl(SourceRange range, std::string function_name,
                 std::vector<Parameter> params,
                 std::unique_ptr<TypeSyntax> result_type,
                 std::unique_ptr<BlockStmt> function_body);

    void dump(std::ostream& out, int indent) const override;
};

struct StructDecl final : Decl {
    std::string name;
    std::vector<FieldDecl> fields;

    StructDecl(SourceRange range, std::string struct_name, std::vector<FieldDecl> field_list);

    void dump(std::ostream& out, int indent) const override;
};

struct TypeAliasDecl final : Decl {
    std::string name;
    std::unique_ptr<TypeSyntax> aliased_type;

    TypeAliasDecl(SourceRange range, std::string alias_name,
                  std::unique_ptr<TypeSyntax> target_type);

    void dump(std::ostream& out, int indent) const override;
};

struct NamespaceDecl final : Decl {
    std::string name;
    std::vector<std::unique_ptr<Decl>> declarations;

    NamespaceDecl(SourceRange range, std::string namespace_name,
                  std::vector<std::unique_ptr<Decl>> nested_declarations);

    void dump(std::ostream& out, int indent) const override;
};

struct VariableDeclStmt final : Stmt {
    Mutability mutability;
    std::unique_ptr<TypeSyntax> type;
    std::string name;
    std::unique_ptr<Expr> initializer;

    VariableDeclStmt(SourceRange range, Mutability variable_mutability,
                     std::unique_ptr<TypeSyntax> variable_type, std::string variable_name,
                     std::unique_ptr<Expr> init_expr);

    void dump(std::ostream& out, int indent) const override;
};

struct IfStmt final : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> then_branch;
    std::unique_ptr<BlockStmt> else_branch;

    IfStmt(SourceRange range, std::unique_ptr<Expr> if_condition,
           std::unique_ptr<BlockStmt> then_block,
           std::unique_ptr<BlockStmt> else_block);

    void dump(std::ostream& out, int indent) const override;
};

struct WhileStmt final : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> body;

    WhileStmt(SourceRange range, std::unique_ptr<Expr> while_condition,
              std::unique_ptr<BlockStmt> loop_body);

    void dump(std::ostream& out, int indent) const override;
};

struct ReturnStmt final : Stmt {
    std::unique_ptr<Expr> value;

    ReturnStmt(SourceRange range, std::unique_ptr<Expr> return_value);

    void dump(std::ostream& out, int indent) const override;
};

struct BreakStmt final : Stmt {
    explicit BreakStmt(SourceRange range);

    void dump(std::ostream& out, int indent) const override;
};

struct ContinueStmt final : Stmt {
    explicit ContinueStmt(SourceRange range);

    void dump(std::ostream& out, int indent) const override;
};

struct ExprStmt final : Stmt {
    std::unique_ptr<Expr> expression;

    ExprStmt(SourceRange range, std::unique_ptr<Expr> expr);

    void dump(std::ostream& out, int indent) const override;
};

struct EmptyStmt final : Stmt {
    explicit EmptyStmt(SourceRange range);

    void dump(std::ostream& out, int indent) const override;
};

struct AssignmentExpr final : Expr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;

    AssignmentExpr(SourceRange range, std::unique_ptr<Expr> assignment_target,
                   std::unique_ptr<Expr> assignment_value);

    void dump(std::ostream& out, int indent) const override;
};

struct BinaryExpr final : Expr {
    TokenType op_type;
    std::string op_lexeme;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(SourceRange range, TokenType type, std::string lexeme,
               std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);

    void dump(std::ostream& out, int indent) const override;
};

struct UnaryExpr final : Expr {
    TokenType op_type;
    std::string op_lexeme;
    std::unique_ptr<Expr> operand;

    UnaryExpr(SourceRange range, TokenType type, std::string lexeme,
              std::unique_ptr<Expr> expr);

    void dump(std::ostream& out, int indent) const override;
};

struct CastExpr final : Expr {
    std::unique_ptr<TypeSyntax> target_type;
    std::unique_ptr<Expr> expression;

    CastExpr(SourceRange range, std::unique_ptr<TypeSyntax> cast_type,
             std::unique_ptr<Expr> cast_expression);

    void dump(std::ostream& out, int indent) const override;
};

struct CallExpr final : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(SourceRange range, std::unique_ptr<Expr> target,
             std::vector<std::unique_ptr<Expr>> args);

    void dump(std::ostream& out, int indent) const override;
};

struct IndexExpr final : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;

    IndexExpr(SourceRange range, std::unique_ptr<Expr> indexed_base,
              std::unique_ptr<Expr> index_expr);

    void dump(std::ostream& out, int indent) const override;
};

struct FieldAccessExpr final : Expr {
    std::unique_ptr<Expr> base;
    std::string field;

    FieldAccessExpr(SourceRange range, std::unique_ptr<Expr> field_base,
                    std::string field_name);

    void dump(std::ostream& out, int indent) const override;
};

struct IdentifierExpr final : Expr {
    std::string name;

    IdentifierExpr(SourceRange range, std::string identifier_name);

    void dump(std::ostream& out, int indent) const override;
};

struct NamespaceAccessExpr final : Expr {
    std::vector<std::string> path;

    NamespaceAccessExpr(SourceRange range, std::vector<std::string> qualified_name);

    void dump(std::ostream& out, int indent) const override;
};

struct IntLiteralExpr final : Expr {
    std::string value;

    IntLiteralExpr(SourceRange range, std::string literal_value);

    void dump(std::ostream& out, int indent) const override;
};

struct FloatLiteralExpr final : Expr {
    std::string value;

    FloatLiteralExpr(SourceRange range, std::string literal_value);

    void dump(std::ostream& out, int indent) const override;
};

struct StringLiteralExpr final : Expr {
    std::string value;

    StringLiteralExpr(SourceRange range, std::string literal_value);

    void dump(std::ostream& out, int indent) const override;
};

struct BoolLiteralExpr final : Expr {
    bool value;

    BoolLiteralExpr(SourceRange range, bool literal_value);

    void dump(std::ostream& out, int indent) const override;
};

struct StructLiteralExpr final : Expr {
    std::vector<std::string> type_path;
    std::vector<FieldInitializer> fields;

    StructLiteralExpr(SourceRange range, std::vector<std::string> type_name,
                      std::vector<FieldInitializer> field_initializers);

    void dump(std::ostream& out, int indent) const override;
};

struct ArrayLiteralExpr final : Expr {
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayLiteralExpr(SourceRange range, std::vector<std::unique_ptr<Expr>> literal_elements);

    void dump(std::ostream& out, int indent) const override;
};

}  


