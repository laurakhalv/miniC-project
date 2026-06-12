#include "lexer.hpp"

#include <cctype>

namespace {

// Эти helpers не зависят от состояния Lexer и просто отвечают на вопросы
// про отдельные символы.
bool is_identifier_start(char ch) {
    const auto byte = static_cast<unsigned char>(ch);
    return std::isalpha(byte) != 0 || ch == '_';
}

bool is_identifier_part(char ch) {
    const auto byte = static_cast<unsigned char>(ch);
    return std::isalnum(byte) != 0 || ch == '_';
}

bool is_digit(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

bool is_hex_digit(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

bool is_binary_digit(char ch) {
    return ch == '0' || ch == '1';
}

Lexer::TokenType identifier_token_type(std::string_view lexeme) {
    using TokenType = Lexer::TokenType;

    // Идентификаторы и ключевые слова читаются одинаково, поэтому сначала
    // считываем всё слово целиком, а потом определяем его класс.
    if (lexeme == "let") {
        return TokenType::Let;
    }
    if (lexeme == "const") {
        return TokenType::Const;
    }
    if (lexeme == "func") {
        return TokenType::Func;
    }
    if (lexeme == "struct") {
        return TokenType::Struct;
    }
    if (lexeme == "type") {
        return TokenType::Type;
    }
    if (lexeme == "namespace") {
        return TokenType::Namespace;
    }
    if (lexeme == "module") {
        return TokenType::Module;
    }
    if (lexeme == "import") {
        return TokenType::Import;
    }
    if (lexeme == "if") {
        return TokenType::If;
    }
    if (lexeme == "else") {
        return TokenType::Else;
    }
    if (lexeme == "while") {
        return TokenType::While;
    }
    if (lexeme == "break") {
        return TokenType::Break;
    }
    if (lexeme == "continue") {
        return TokenType::Continue;
    }
    if (lexeme == "return") {
        return TokenType::Return;
    }
    if (lexeme == "cast") {
        return TokenType::Cast;
    }
    if (lexeme == "operator") {
        return TokenType::Operator;
    }
    if (lexeme == "true") {
        return TokenType::True;
    }
    if (lexeme == "false") {
        return TokenType::False;
    }
    if (lexeme == "public") {
        return TokenType::Public;
    }
    if (lexeme == "private") {
        return TokenType::Private;
    }
    if (lexeme == "int8") {
        return TokenType::Int8;
    }
    if (lexeme == "int16") {
        return TokenType::Int16;
    }
    if (lexeme == "int32") {
        return TokenType::Int32;
    }
    if (lexeme == "int64") {
        return TokenType::Int64;
    }
    if (lexeme == "uint8") {
        return TokenType::UInt8;
    }
    if (lexeme == "uint16") {
        return TokenType::UInt16;
    }
    if (lexeme == "uint32") {
        return TokenType::UInt32;
    }
    if (lexeme == "uint64") {
        return TokenType::UInt64;
    }
    if (lexeme == "float32") {
        return TokenType::Float32;
    }
    if (lexeme == "float64") {
        return TokenType::Float64;
    }
    if (lexeme == "bool") {
        return TokenType::Bool;
    }
    if (lexeme == "char") {
        return TokenType::Char;
    }
    if (lexeme == "string") {
        return TokenType::String;
    }
    if (lexeme == "void") {
        return TokenType::Void;
    }
    if (lexeme == "inf" || lexeme == "NaN") {
        return TokenType::FloatLiteral;
    }

    return TokenType::Identifier;
}

}  // namespace

namespace Lexer {

Lexer::Lexer(std::string_view source, std::string filename)
    : source_(source), filename_(std::move(filename)) {}

std::expected<Token, LexError> Lexer::next_token() {
    auto skipped = skip_whitespace_and_comments();
    if (!skipped) {
        return std::unexpected(skipped.error());
    }

    if (is_at_end()) {
        return make_eof_token();
    }

    // После пропуска пробелов и комментариев достаточно первого символа,
    // чтобы выбрать нужную ветку лексического анализа.
    if (is_identifier_start(peek())) {
        return lex_identifier_or_keyword();
    }

    if (is_digit(peek())) {
        return lex_number();
    }

    if (peek() == '"') {
        return lex_string();
    }

    if (peek() == '\'') {
        return lex_char();
    }

    const SourceLocation start = location_;
    const char ch = advance();

    switch (ch) {
        case '+':
            return make_token(TokenType::Plus, start);
        case '-':
            if (match('>')) {
                return make_token(TokenType::Arrow, start);
            }
            return make_token(TokenType::Minus, start);
        case '*':
            return make_token(TokenType::Star, start);
        case '/':
            return make_token(TokenType::Slash, start);
        case '%':
            return make_token(TokenType::Percent, start);
        case '^':
            return make_token(TokenType::Caret, start);
        case '=':
            if (match('=')) {
                return make_token(TokenType::EqualEqual, start);
            }
            return make_token(TokenType::Assign, start);
        case '!':
            if (match('=')) {
                return make_token(TokenType::BangEqual, start);
            }
            return make_token(TokenType::Bang, start);
        case '<':
            if (match('=')) {
                return make_token(TokenType::LessEqual, start);
            }
            if (match('<')) {
                return make_token(TokenType::ShiftLeft, start);
            }
            return make_token(TokenType::Less, start);
        case '>':
            if (match('=')) {
                return make_token(TokenType::GreaterEqual, start);
            }
            if (match('>')) {
                return make_token(TokenType::ShiftRight, start);
            }
            return make_token(TokenType::Greater, start);
        case '&':
            if (match('&')) {
                return make_token(TokenType::AmpAmp, start);
            }
            return make_token(TokenType::Amp, start);
        case '|':
            if (match('|')) {
                return make_token(TokenType::PipePipe, start);
            }
            if (match('>')) {
                return make_token(TokenType::PipeGreater, start);
            }
            return make_token(TokenType::Pipe, start);
        case '~':
            return make_token(TokenType::Tilde, start);
        case '(':
            return make_token(TokenType::LeftParen, start);
        case ')':
            return make_token(TokenType::RightParen, start);
        case '{':
            return make_token(TokenType::LeftBrace, start);
        case '}':
            return make_token(TokenType::RightBrace, start);
        case '[':
            return make_token(TokenType::LeftBracket, start);
        case ']':
            return make_token(TokenType::RightBracket, start);
        case ';':
            return make_token(TokenType::Semicolon, start);
        case ',':
            return make_token(TokenType::Comma, start);
        case '.':
            return make_token(TokenType::Dot, start);
        case ':':
            if (match(':')) {
                return make_token(TokenType::ColonColon, start);
            }
            return make_token(TokenType::Colon, start);
        default:
            return std::unexpected(make_error(std::string("unexpected character '") + ch + "'", start));
    }
}

std::expected<std::vector<Token>, LexError> Lexer::tokenize() {
    std::vector<Token> tokens;

    // next_token() возвращает токены по одному, а tokenize() просто
    // собирает их в вектор до EndOfFile.
    while (true) {
        auto token_or_error = next_token();
        if (!token_or_error) {
            return std::unexpected(token_or_error.error());
        }

        tokens.push_back(std::move(token_or_error.value()));
        if (tokens.back().type == TokenType::EndOfFile) {
            break;
        }
    }

    return tokens;
}

bool Lexer::is_at_end() const {
    return index_ >= source_.size();
}

char Lexer::peek() const {
    if (is_at_end()) {
        return '\0';
    }
    return source_[index_];
}

char Lexer::peek_next() const {
    if (index_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[index_ + 1];
}

char Lexer::advance() {
    const char ch = source_[index_++];
    ++location_.offset;

    // Позиция обновляется на каждом символе, чтобы ошибки потом можно было
    // привязать к точному месту в исходнике.
    if (ch == '\n') {
        ++location_.line;
        location_.column = 1;
    } else {
        ++location_.column;
    }

    return ch;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[index_] != expected) {
        return false;
    }

    advance();
    return true;
}

std::expected<void, LexError> Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        const char ch = peek();

        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            advance();
            continue;
        }

        // В языке поддерживаются только однострочные комментарии //.
        // Всё до конца строки просто пропускается.
        if (ch == '/' && peek_next() == '/') {
            advance();
            advance();

            while (!is_at_end() && peek() != '\n') {
                advance();
            }

            continue;
        }

        if (ch == '/' && peek_next() == '*') {
            const SourceLocation comment_start = location_;
            advance();
            advance();

            int depth = 1;
            while (depth > 0) {
                if (is_at_end()) {
                    return std::unexpected(
                        make_error("unterminated block comment", comment_start));
                }

                if (peek() == '/' && peek_next() == '*') {
                    advance();
                    advance();
                    ++depth;
                    continue;
                }

                if (peek() == '*' && peek_next() == '/') {
                    advance();
                    advance();
                    --depth;
                    continue;
                }

                advance();
            }

            continue;
        }

        break;
    }

    return {};
}

std::expected<Token, LexError> Lexer::lex_identifier_or_keyword() {
    const SourceLocation start = location_;

    // Считываем максимально длинную последовательность символов идентификатора.
    while (is_identifier_part(peek())) {
        advance();
    }

    const auto lexeme = source_.substr(start.offset, location_.offset - start.offset);
    return make_token(identifier_token_type(lexeme), start);
}

std::expected<Token, LexError> Lexer::lex_number() {
    const SourceLocation start = location_;

    if (peek() == '0' && (peek_next() == 'x' || peek_next() == 'X')) {
        advance();
        advance();
        if (!is_hex_digit(peek())) {
            return std::unexpected(make_error("expected hexadecimal digits after '0x'", location_));
        }
        while (is_hex_digit(peek())) {
            advance();
        }
        return make_token(TokenType::IntLiteral, start);
    }

    if (peek() == '0' && (peek_next() == 'b' || peek_next() == 'B')) {
        advance();
        advance();
        if (!is_binary_digit(peek())) {
            return std::unexpected(make_error("expected binary digits after '0b'", location_));
        }
        while (is_binary_digit(peek())) {
            advance();
        }
        return make_token(TokenType::IntLiteral, start);
    }

    // Сначала читается целая часть числа.
    while (is_digit(peek())) {
        advance();
    }

    TokenType type = TokenType::IntLiteral;

    // Число считается вещественным только если после точки есть хотя бы
    // одна цифра. Из-за этого "1." не принимается как float literal.
    if (peek() == '.' && is_digit(peek_next())) {
        type = TokenType::FloatLiteral;
        advance();

        while (is_digit(peek())) {
            advance();
        }
    }

    if (peek() == 'e' || peek() == 'E') {
        const auto exponent_mark = index_;
        const auto exponent_location = location_;
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }

        if (!is_digit(peek())) {
            index_ = exponent_mark;
            location_ = exponent_location;
        } else {
            type = TokenType::FloatLiteral;
            while (is_digit(peek())) {
                advance();
            }
        }
    }

    return make_token(type, start);
}

std::expected<Token, LexError> Lexer::lex_string() {
    const SourceLocation start = location_;
    // Пропускаем открывающую кавычку и начинаем читать содержимое строки.
    advance();

    while (!is_at_end()) {
        const char ch = peek();

        if (ch == '"') {
            advance();
            return make_token(TokenType::StringLiteral, start);
        }

        if (ch == '\n') {
            return std::unexpected(make_error("unterminated string literal", location_));
        }

        if (ch == '\\') {
            const SourceLocation escape_location = location_;
            advance();

            if (is_at_end()) {
                return std::unexpected(make_error("unterminated string literal", location_));
            }

            // Escape-последовательности проверяются уже на этапе лексера,
            // чтобы дальше parser и semantic работали только с валидными
            // строковыми литералами.
            const char escaped = advance();
            if (escaped != '"' && escaped != '\\' && escaped != 'n' && escaped != 't') {
                return std::unexpected(make_error("invalid escape sequence", escape_location));
            }

            continue;
        }

        advance();
    }

    return std::unexpected(make_error("unterminated string literal", start));
}

std::expected<Token, LexError> Lexer::lex_char() {
    const SourceLocation start = location_;
    advance();

    if (is_at_end() || peek() == '\n') {
        return std::unexpected(make_error("unterminated character literal", start));
    }

    if (peek() == '\\') {
        const SourceLocation escape_location = location_;
        advance();
        if (is_at_end()) {
            return std::unexpected(make_error("unterminated character literal", start));
        }

        const char escaped = advance();
        if (escaped != '\'' && escaped != '"' && escaped != '\\' && escaped != 'n' &&
            escaped != 't') {
            return std::unexpected(make_error("invalid escape sequence", escape_location));
        }
    } else {
        advance();
    }

    if (!match('\'')) {
        return std::unexpected(make_error("character literal must contain exactly one character",
                                          start));
    }

    return make_token(TokenType::CharLiteral, start);
}

Token Lexer::make_token(TokenType type, SourceLocation start) const {
    // lexeme вырезается прямо из исходного текста по диапазону [start, current).
    return Token {
        .type = type,
        .lexeme = std::string(source_.substr(start.offset, location_.offset - start.offset)),
        .range = SourceRange {start, location_},
    };
}

Token Lexer::make_eof_token() const {
    // EOF не занимает символов, поэтому begin и end совпадают.
    return Token {
        .type = TokenType::EndOfFile,
        .lexeme = "",
        .range = SourceRange {location_, location_},
    };
}

LexError Lexer::make_error(std::string message, SourceLocation location) const {
    // Здесь собираем структуру ошибки, а в main.cpp она уже форматируется для вывода.
    return LexError {
        .filename = filename_,
        .message = std::move(message),
        .location = location,
    };
}

std::expected<std::vector<Token>, LexError> tokenize(std::string_view source, std::string filename) {
    Lexer lexer(source, std::move(filename));
    return lexer.tokenize();
}

}  // namespace Lexer
