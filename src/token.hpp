#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Lexer {

//позиция одного символа в исходном файле
struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t offset = 0;
};

struct SourceRange {
    SourceLocation begin {};
    SourceLocation end {};
};

enum class TokenType {
    // базовые классы токенов
    EndOfFile,
    Identifier,
    IntLiteral,
    FloatLiteral,
    StringLiteral,

    // ключевые слова
    Let,
    Const,
    Func,
    Struct,
    Type,
    Namespace,
    If,
    Else,
    While,
    Break,
    Continue,
    Return,
    Cast,
    True,
    False,

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
    String,
    Void,

    //операторы и разделители
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Assign,
    EqualEqual,
    BangEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    AmpAmp,
    PipePipe,
    Bang,
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

struct Token {
    TokenType type = TokenType::EndOfFile;
    std::string lexeme {};
    SourceRange range {};
};

struct LexError {
    std::string filename {};
    std::string message {};
    SourceLocation location {};
};

std::string_view to_string(TokenType type);
std::string format_error(const LexError& error);

}