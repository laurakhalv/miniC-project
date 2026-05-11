#include "token.hpp"

#include <sstream>

namespace Lexer {

std::string_view to_string(TokenType type) {
    //нужен для отладки и для флага 
    switch (type) {
        case TokenType::EndOfFile:
            return "EndOfFile";
        case TokenType::Identifier:
            return "Identifier";
        case TokenType::IntLiteral:
            return "IntLiteral";
        case TokenType::FloatLiteral:
            return "FloatLiteral";
        case TokenType::StringLiteral:
            return "StringLiteral";
        case TokenType::Let:
            return "Let";
        case TokenType::Const:
            return "Const";
        case TokenType::Func:
            return "Func";
        case TokenType::Struct:
            return "Struct";
        case TokenType::Type:
            return "Type";
        case TokenType::Namespace:
            return "Namespace";
        case TokenType::If:
            return "If";
        case TokenType::Else:
            return "Else";
        case TokenType::While:
            return "While";
        case TokenType::Break:
            return "Break";
        case TokenType::Continue:
            return "Continue";
        case TokenType::Return:
            return "Return";
        case TokenType::Cast:
            return "Cast";
        case TokenType::True:
            return "True";
        case TokenType::False:
            return "False";
        case TokenType::Int8:
            return "Int8";
        case TokenType::Int16:
            return "Int16";
        case TokenType::Int32:
            return "Int32";
        case TokenType::Int64:
            return "Int64";
        case TokenType::UInt8:
            return "UInt8";
        case TokenType::UInt16:
            return "UInt16";
        case TokenType::UInt32:
            return "UInt32";
        case TokenType::UInt64:
            return "UInt64";
        case TokenType::Float32:
            return "Float32";
        case TokenType::Float64:
            return "Float64";
        case TokenType::Bool:
            return "Bool";
        case TokenType::String:
            return "String";
        case TokenType::Void:
            return "Void";
        case TokenType::Plus:
            return "Plus";
        case TokenType::Minus:
            return "Minus";
        case TokenType::Star:
            return "Star";
        case TokenType::Slash:
            return "Slash";
        case TokenType::Percent:
            return "Percent";
        case TokenType::Assign:
            return "Assign";
        case TokenType::EqualEqual:
            return "EqualEqual";
        case TokenType::BangEqual:
            return "BangEqual";
        case TokenType::Less:
            return "Less";
        case TokenType::Greater:
            return "Greater";
        case TokenType::LessEqual:
            return "LessEqual";
        case TokenType::GreaterEqual:
            return "GreaterEqual";
        case TokenType::AmpAmp:
            return "AmpAmp";
        case TokenType::PipePipe:
            return "PipePipe";
        case TokenType::Bang:
            return "Bang";
        case TokenType::LeftParen:
            return "LeftParen";
        case TokenType::RightParen:
            return "RightParen";
        case TokenType::LeftBrace:
            return "LeftBrace";
        case TokenType::RightBrace:
            return "RightBrace";
        case TokenType::LeftBracket:
            return "LeftBracket";
        case TokenType::RightBracket:
            return "RightBracket";
        case TokenType::Semicolon:
            return "Semicolon";
        case TokenType::Comma:
            return "Comma";
        case TokenType::Dot:
            return "Dot";
        case TokenType::Colon:
            return "Colon";
        case TokenType::ColonColon:
            return "ColonColon";
        case TokenType::Arrow:
            return "Arrow";
    }

    return "Unknown";
}

std::string format_error(const LexError& error) {
    // все лексические ошибки приводятся к единому формату diagnostics
    std::ostringstream stream;
    stream << error.filename << ':' << error.location.line << ':' << error.location.column
           << ": error: " << error.message;
    return stream.str();
}

}  