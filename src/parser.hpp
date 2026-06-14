#pragma once

#include "ast.hpp"
#include "token.hpp"
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace Parser {

struct ParseError {
    std::string filename {};
    std::string message {};
    Lexer::SourceLocation location {};
};

std::string format_error(const ParseError& error);

class Parser {
  public:
    explicit Parser(std::vector<Lexer::Token> tokens, std::string filename = "<memory>");

    // точка входа всего parser: разбирает весь поток токенов в Program
    std::expected<AST::Program, ParseError> parse_program();

  private:
    using Token = Lexer::Token;
    using TokenType = Lexer::TokenType;

    // удобное представление квалифицированного имени вида A::B::C
    struct ParsedPath {
        std::vector<std::string> parts;
        Lexer::SourceRange range {};
    };

    std::vector<Token> tokens_;
    std::string filename_;
  // current_ всегда указывает на токен, который parser ещё не "съел"
    std::size_t current_ = 0;

    bool is_at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool check_next(TokenType type) const;
    bool match(TokenType type);
    bool match_any(std::initializer_list<TokenType> types);

    std::expected<Token, ParseError> consume(TokenType type, std::string message);
    ParseError make_error(const Token& token, std::string message) const;

    //разбор верхнего уровня программы
    std::expected<std::unique_ptr<AST::Decl>, ParseError> parse_declaration(
    bool default_exported = false);
    std::expected<std::unique_ptr<AST::FunctionDecl>, ParseError> parse_function_decl(
        bool is_method = false, AST::Visibility visibility = AST::Visibility::Public);
    std::expected<std::unique_ptr<AST::StructDecl>, ParseError> parse_struct_decl();
    std::expected<std::unique_ptr<AST::TypeAliasDecl>, ParseError> parse_type_alias_decl();
    std::expected<std::unique_ptr<AST::NamespaceDecl>, ParseError> parse_namespace_decl(
    bool default_exported = false);

    //разбор синтаксиса типов и квалифицированных имён
    std::expected<std::unique_ptr<AST::TypeSyntax>, ParseError> parse_type();
    std::expected<ParsedPath, ParseError> parse_identifier_path();
    std::expected<ParsedPath, ParseError> parse_module_path();

    // разбор инструкций
    std::expected<std::unique_ptr<AST::BlockStmt>, ParseError> parse_block();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_variable_decl_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_if_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_while_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_return_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_break_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_continue_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_expression_statement();
    std::expected<std::unique_ptr<AST::Stmt>, ParseError> parse_empty_statement();

    // разбор выражений идёт по уровням приоритета операторов
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_assignment_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_pipe_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_logical_or_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_logical_and_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_bitwise_or_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_bitwise_xor_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_bitwise_and_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_equality_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_relational_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_shift_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_additive_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_multiplicative_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_unary_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_cast_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_postfix_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_if_expression();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_atom();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_array_literal();
    std::expected<std::unique_ptr<AST::Expr>, ParseError> parse_struct_literal(ParsedPath type_path);

    // эти helpers помогают parser быстро понять, какой синтаксис ждать дальше
    bool is_type_start() const;
    bool is_builtin_type_token(TokenType type) const;
    // рроверка, что выражение допустимо слева от оператора присваивания
    bool is_assignable_expression(const AST::Expr& expr) const;
};

}
