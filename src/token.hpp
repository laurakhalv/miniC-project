#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Lexer {

// Позиция одного символа в исходном файле.
struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t offset = 0;
};

// Полуинтервал [begin, end), который занимает токен или AST-узел.
struct SourceRange {
    SourceLocation begin {};
    SourceLocation end {};
};

enum class TokenType {
    // Базовые классы токенов.
    EndOfFile,
    Identifier,
    IntLiteral,
    FloatLiteral,
    CharLiteral,
    StringLiteral,

    // Ключевые слова.
    Let,
    Const,
    Func,
    Struct,
    Type,
    Namespace,
    Module,
    Import,
    If,
    Else,
    While,
    Break,
    Continue,
    Return,
    Cast,
    Operator,
    True,
    False,
    Public,
    Private,

    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float32,
    Float64,
    Bool,
    Char,
    String,
    Void,

    // Операторы и разделители.
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Amp,
    Pipe,
    Caret,
    Assign,
    EqualEqual,
    BangEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    ShiftLeft,
    ShiftRight,
    AmpAmp,
    PipePipe,
    PipeGreater,
    Bang,
    Tilde,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Semicolon,
    Comma,
    Dot,
    Colon,
    ColonColon,
    Arrow,
};

// Token - это результат работы лексера: класс токена, его текст и место в исходнике.
struct Token {
    TokenType type = TokenType::EndOfFile;
    std::string lexeme {};
    SourceRange range {};
};

// LexError хранит уже готовую информацию для diagnostics.
struct LexError {
    std::string filename {};
    std::string message {};
    SourceLocation location {};
};

std::string_view to_string(TokenType type);
std::string format_error(const LexError& error);

}