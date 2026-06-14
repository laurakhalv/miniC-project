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

    // dump() используется для --dump-ast и печатает дерево в читаемом виде
    virtual void dump(std::ostream& out, int indent) const = 0;
};

enum class Mutability {
    Mutable,
    Immutable,
};

enum class Visibility {
    Public,
    Private,
};
//объявление
struct Decl : Node {
    using Node::Node;
    // ( Доп 3) объявление может быть экспортируемым
    // или приватным относительно других модулей.
    bool is_exported = false;
    bool has_module_visibility = false;
};

// инструкция
struct Stmt : Node {
    using Node::Node;
};

// выражения
struct Expr : Node {
    using Node::Node;
};

// хранит имя типа в том виде, как оно записано в программе
//  int32, Point, Math::Point, int32[3]
struct TypeSyntax final : Node {
    std::vector<std::string> name_parts;
    std::optional<std::string> array_size;

    TypeSyntax(SourceRange range, std::vector<std::string> parts,
               std::optional<std::string> size = std::nullopt);

    void dump(std::ostream& out, int indent) const override;
};

//один параметр функции
struct Parameter {
    std::unique_ptr<TypeSyntax> type;
    std::string name;
    std::unique_ptr<Expr> default_value;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

//oдно поле структуры
struct FieldDecl {
    std::unique_ptr<TypeSyntax> type;
    std::string name;
    Visibility visibility = Visibility::Public;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

//инициализация одного поля при создании структуры
struct FieldInitializer {
    std::string name;
    std::unique_ptr<Expr> value;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

struct ImportSpec {
    std::vector<std::string> module_path;
    std::optional<std::vector<std::string>> imported_path;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

//корень всего дерева 
struct Program {
    // declarations список всех объявлений верхнего уровня
    std::optional<std::vector<std::string>> module_name;
    std::vector<ImportSpec> imports;
    std::vector<std::unique_ptr<Decl>> declarations;

    void dump(std::ostream& out) const;
};

//вводит локальную область видимости и содержит последовательность
// инструкций внутри фигурных скобок
struct BlockStmt final : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    BlockStmt(SourceRange range, std::vector<std::unique_ptr<Stmt>> body_statements);

    void dump(std::ostream& out, int indent) const override;
};

//обьявление функции
struct FunctionDecl final : Decl {
    std::string name;
    std::vector<Parameter> parameters;
    std::unique_ptr<TypeSyntax> return_type;
    std::unique_ptr<BlockStmt> body;
    bool is_method = false;
    Visibility visibility = Visibility::Public;

    FunctionDecl(SourceRange range, std::string function_name,
                 std::vector<Parameter> params,
                 std::unique_ptr<TypeSyntax> result_type,
                 std::unique_ptr<BlockStmt> function_body,
                 bool method = false,
                 Visibility member_visibility = Visibility::Public);

    void dump(std::ostream& out, int indent) const override;
};

//обьявление структуры 
struct StructDecl final : Decl {
    std::string name;
    std::vector<FieldDecl> fields;
    std::vector<std::unique_ptr<FunctionDecl>> methods;

    StructDecl(SourceRange range, std::string struct_name, std::vector<FieldDecl> field_list,
               std::vector<std::unique_ptr<FunctionDecl>> method_list);

    void dump(std::ostream& out, int indent) const override;
};

//тип alias (синоним типа) 
struct TypeAliasDecl final : Decl {
    std::string name;
    std::unique_ptr<TypeSyntax> aliased_type;

    TypeAliasDecl(SourceRange range, std::string alias_name,
                  std::unique_ptr<TypeSyntax> target_type);

    void dump(std::ostream& out, int indent) const override;
};
//пространство имён; namespace Math { func square(...) {} }
struct NamespaceDecl final : Decl {
    std::string name;
    std::vector<std::unique_ptr<Decl>> declarations;

    NamespaceDecl(SourceRange range, std::string namespace_name,
                  std::vector<std::unique_ptr<Decl>> nested_declarations);

    void dump(std::ostream& out, int indent) const override;
};
//oбъявление переменной
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

//yсловная инструкция: if (x > 0) { ... } else { ... }
struct IfStmt final : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> then_branch;
    std::unique_ptr<BlockStmt> else_branch;

    IfStmt(SourceRange range, std::unique_ptr<Expr> if_condition,
           std::unique_ptr<BlockStmt> then_block,
           std::unique_ptr<BlockStmt> else_block);

    void dump(std::ostream& out, int indent) const override;
};

//цикл: while (x > 0) { ... }
struct WhileStmt final : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> body;

    WhileStmt(SourceRange range, std::unique_ptr<Expr> while_condition,
              std::unique_ptr<BlockStmt> loop_body);

    void dump(std::ostream& out, int indent) const override;
};

//возврат из функции
struct ReturnStmt final : Stmt {
    std::unique_ptr<Expr> value;

    ReturnStmt(SourceRange range, std::unique_ptr<Expr> return_value);

    void dump(std::ostream& out, int indent) const override;
};
//break
struct BreakStmt final : Stmt {
    explicit BreakStmt(SourceRange range);

    void dump(std::ostream& out, int indent) const override;
};
//continue
struct ContinueStmt final : Stmt {
    explicit ContinueStmt(SourceRange range);

    void dump(std::ostream& out, int indent) const override;
};

//x=5;
struct ExprStmt final : Stmt {
    std::unique_ptr<Expr> expression;

    ExprStmt(SourceRange range, std::unique_ptr<Expr> expr);

    void dump(std::ostream& out, int indent) const override;
};
//; (пустая инструкция)
struct EmptyStmt final : Stmt {
    explicit EmptyStmt(SourceRange range);

    void dump(std::ostream& out, int indent) const override;
};

//присваивание
struct AssignmentExpr final : Expr {
    // target -что присваиваем, value -что записываем
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;

    AssignmentExpr(SourceRange range, std::unique_ptr<Expr> assignment_target,
                   std::unique_ptr<Expr> assignment_value);

    void dump(std::ostream& out, int indent) const override;
};

//бинарная операция
struct BinaryExpr final : Expr {
    // в BinaryExpr сохраняем и вид оператора, и его текстовую форму
    TokenType op_type;
    std::string op_lexeme;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(SourceRange range, TokenType type, std::string lexeme,
               std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);

    void dump(std::ostream& out, int indent) const override;
};

//унарная
struct UnaryExpr final : Expr {
    TokenType op_type;
    std::string op_lexeme;
    std::unique_ptr<Expr> operand;

    UnaryExpr(SourceRange range, TokenType type, std::string lexeme,
              std::unique_ptr<Expr> expr);

    void dump(std::ostream& out, int indent) const override;
};

//явное привидение типа
struct CastExpr final : Expr {
    std::unique_ptr<TypeSyntax> target_type;
    std::unique_ptr<Expr> expression;

    CastExpr(SourceRange range, std::unique_ptr<TypeSyntax> cast_type,
             std::unique_ptr<Expr> cast_expression);

    void dump(std::ostream& out, int indent) const override;
};
//один аргумент вызова функции (доп A.2.10)
struct CallArgument {
    std::optional<std::string> name;
    std::unique_ptr<Expr> value;
    SourceRange range {};

    void dump(std::ostream& out, int indent) const;
};

//вызов функции
struct CallExpr final : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<CallArgument> arguments;

    CallExpr(SourceRange range, std::unique_ptr<Expr> target,
             std::vector<CallArgument> args);

    void dump(std::ostream& out, int indent) const override;
};
//индексирование массива
struct IndexExpr final : Expr {
    std::unique_ptr<Expr> base;
    std::unique_ptr<Expr> index;

    IndexExpr(SourceRange range, std::unique_ptr<Expr> indexed_base,
              std::unique_ptr<Expr> index_expr);

    void dump(std::ostream& out, int indent) const override;
};

//доступ к полю структуры: p.x, obj.name
struct FieldAccessExpr final : Expr {
    std::unique_ptr<Expr> base;
    std::string field;

    FieldAccessExpr(SourceRange range, std::unique_ptr<Expr> field_base,
                    std::string field_name);

    void dump(std::ostream& out, int indent) const override;
};
//oбращение к переменной
struct IdentifierExpr final : Expr {
    // имя пока хранится как строка (смысл имени позже определяет семантика)
    std::string name;
    IdentifierExpr(SourceRange range, std::string identifier_name);
    void dump(std::ostream& out, int indent) const override;
};

//обращение через ::: Math::pi
struct NamespaceAccessExpr final : Expr {
    std::vector<std::string> path;

    NamespaceAccessExpr(SourceRange range, std::vector<std::string> qualified_name);

    void dump(std::ostream& out, int indent) const override;
};
  //литеральные выражение всехранят значение как строку
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

struct CharLiteralExpr final : Expr {
    std::string value;

    CharLiteralExpr(SourceRange range, std::string literal_value);

    void dump(std::ostream& out, int indent) const override;
};

struct BoolLiteralExpr final : Expr {
    bool value;

    BoolLiteralExpr(SourceRange range, bool literal_value);

    void dump(std::ostream& out, int indent) const override;
};

// (доп A.2.1) if-выражение 
struct IfExpr final : Expr {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> then_branch;
    std::unique_ptr<Expr> else_branch;

    IfExpr(SourceRange range, std::unique_ptr<Expr> if_condition,
           std::unique_ptr<Expr> then_expr,
           std::unique_ptr<Expr> else_expr);

    void dump(std::ostream& out, int indent) const override;
};

//создание структуры: Point { x: 1, y: 2 } или Math::Point { x: 3, y: 4 }
struct StructLiteralExpr final : Expr {
    // type_path хранит путь к типу буквально так, как он был записан в программе
    std::vector<std::string> type_path;
    std::vector<FieldInitializer> fields;

    StructLiteralExpr(SourceRange range, std::vector<std::string> type_name,
                      std::vector<FieldInitializer> field_initializers);

    void dump(std::ostream& out, int indent) const override;
};
//литерал массива
struct ArrayLiteralExpr final : Expr {
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayLiteralExpr(SourceRange range, std::vector<std::unique_ptr<Expr>> literal_elements);

    void dump(std::ostream& out, int indent) const override;
};

} 
