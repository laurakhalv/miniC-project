#pragma once

#include "token.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace Lexer {

// Lexer читает исходный текст посимвольно и превращает его в поток Token
class Lexer {
  public:
    explicit Lexer(std::string_view source, std::string filename = "<memory>");

    // Возвращает один следующий токен или лексическую ошибку
    std::expected<Token, LexError> next_token();
    // Удобный helper: лексит весь файл целиком до EndOfFile
    std::expected<std::vector<Token>, LexError> tokenize();

  private:
    // source_ - весь текст программы, index_ -текущая позиция в строке
    // location_ дублирует позицию в более удобном для diagnostics виде
    std::string_view source_;
    std::string filename_;
    std::size_t index_ = 0;
    SourceLocation location_ {};

    bool is_at_end() const;
    char peek() const;
    char peek_next() const;
    char advance();
    bool match(char expected);

    // Пропускает пробелы и комментарии, чтобы next_token() работал уже
    // только с содержательными символами языка
    void skip_whitespace_and_comments();

    std::expected<Token, LexError> lex_identifier_or_keyword();
    std::expected<Token, LexError> lex_number();
    std::expected<Token, LexError> lex_char();
    std::expected<Token, LexError> lex_string();

    Token make_token(TokenType type, SourceLocation start) const;
    Token make_eof_token() const;
    LexError make_error(std::string message, SourceLocation location) const;
};

std::expected<std::vector<Token>, LexError> tokenize(std::string_view source,
                                                     std::string filename = "<memory>");

} 