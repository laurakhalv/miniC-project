#include "parser.hpp"

#include <sstream>
#include <utility>

namespace {

// объединяет два диапазона позиций в один , берёт начало левого и конец правого
Lexer::SourceRange combine_ranges(Lexer::SourceRange left, Lexer::SourceRange right) {
    return Lexer::SourceRange {.begin = left.begin, .end = right.end};
}

//(доп A.2.9) переводит тип токена оператора в внутреннее имя функции-оператора
std::optional<std::string> operator_function_name(Lexer::TokenType type) {
    using TokenType = Lexer::TokenType;
    switch (type) {
        case TokenType::Plus:
            return "operator+";
        case TokenType::Minus:
            return "operator-";
        case TokenType::Star:
            return "operator*";
        case TokenType::Slash:
            return "operator/";
        case TokenType::Percent:
            return "operator%";
        case TokenType::EqualEqual:
            return "operator==";
        case TokenType::BangEqual:
            return "operator!=";
        case TokenType::Less:
            return "operator<";
        case TokenType::Greater:
            return "operator>";
        case TokenType::LessEqual:
            return "operator<=";
        case TokenType::GreaterEqual:
            return "operator>=";
        case TokenType::Amp:
            return "operator&";
        case TokenType::Pipe:
            return "operator|";
        case TokenType::Caret:
            return "operator^";
        case TokenType::Tilde:
            return "operator~";
        case TokenType::Bang:
            return "operator!";
        case TokenType::ShiftLeft:
            return "operator<<";
        case TokenType::ShiftRight:
            return "operator>>";
        default:
            return std::nullopt;
    }
}

//реализует конвейерный оператор |> (доп A.1.9)
std::unique_ptr<AST::Expr> lower_pipe_expression(std::unique_ptr<AST::Expr> left,
                                                 std::unique_ptr<AST::Expr> right,
                                                 Lexer::SourceRange range) {
    if (auto* call = dynamic_cast<AST::CallExpr*>(right.get())) {
        std::vector<AST::CallArgument> arguments;
        arguments.push_back(AST::CallArgument {
            .name = std::nullopt,
            .value = std::move(left),
            .range = range,
        });
        for (auto& argument : call->arguments) {
            arguments.push_back(std::move(argument));
        }

        return std::unique_ptr<AST::Expr>(std::make_unique<AST::CallExpr>(
            range, std::move(call->callee), std::move(arguments)));
    }

    std::vector<AST::CallArgument> arguments;
    arguments.push_back(AST::CallArgument {
        .name = std::nullopt,
        .value = std::move(left),
        .range = range,
    });
    return std::unique_ptr<AST::Expr>(
        std::make_unique<AST::CallExpr>(range, std::move(right), std::move(arguments)));
}

} 

namespace Parser {

std::string format_error(const ParseError& error) {
    std::ostringstream stream;
    stream << error.filename << ':' << error.location.line << ':' << error.location.column
           << ": error: " << error.message;
    return stream.str();
}

//принимает уже готовый список токенов от лексера
Parser::Parser(std::vector<Lexer::Token> tokens, std::string filename)
    : tokens_(std::move(tokens)), filename_(std::move(filename)) {}

    //точка входа парсера. разбирает весь файл
std::expected<AST::Program, ParseError> Parser::parse_program() {
    AST::Program program;

    if (match(TokenType::Module)) {
        auto module_path = parse_module_path();
        if (!module_path) {
            return std::unexpected(module_path.error());
        }
        auto semicolon = consume(TokenType::Semicolon, "expected ';' after module declaration");
        if (!semicolon) {
            return std::unexpected(semicolon.error());
        }
        program.module_name = std::move(module_path->parts);
    }

    const bool default_exported = program.module_name.has_value();

   while (match(TokenType::Import)) {
        const auto import_token = previous();
        auto module_path = parse_module_path();
        if (!module_path) {
            return std::unexpected(module_path.error());
        }

        std::optional<std::vector<std::string>> imported_path;
        if (match(TokenType::ColonColon)) {
            auto path = parse_identifier_path();
            if (!path) {
                return std::unexpected(path.error());
            }
            imported_path = std::move(path->parts);
        }

        auto semicolon = consume(TokenType::Semicolon, "expected ';' after import declaration");
        if (!semicolon) {
            return std::unexpected(semicolon.error());
        }

        program.imports.push_back(AST::ImportSpec {
            .module_path = std::move(module_path->parts),
            .imported_path = std::move(imported_path),
            .range = combine_ranges(import_token.range, semicolon->range),
        });
    }

    while (!is_at_end()) {
        auto declaration_or_error = parse_declaration(default_exported);
        if (!declaration_or_error) {
            return std::unexpected(declaration_or_error.error());
        }

        program.declarations.push_back(std::move(declaration_or_error.value()));
    }

    return program;
}


bool Parser::is_at_end() const {
    return peek().type == TokenType::EndOfFile;
}
//возвращает текущий токен без продвижения
const Parser::Token& Parser::peek() const {
    return tokens_[current_];
}

//возвращает предыдущий уже прочитанный токен
const Parser::Token& Parser::previous() const {
    return tokens_[current_ - 1];
}
//возвращает токен, который был только что прочитан
const Parser::Token& Parser::advance() {
    if (!is_at_end()) {
        ++current_;
    }
    return previous();
}

//проверяет тип текущего токена
bool Parser::check(TokenType type) const {
    if (is_at_end()) {
        return type == TokenType::EndOfFile;
    }
    return peek().type == type;
}

//какой тип у следующего токена
bool Parser::check_next(TokenType type) const {
    if (current_ + 1 >= tokens_.size()) {
        return type == TokenType::EndOfFile;
    }
    return tokens_[current_ + 1].type == type;
}

bool Parser::match(TokenType type) {
    //одновременно проверяет токен и продвигает parser вперёд
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

bool Parser::match_any(std::initializer_list<TokenType> types) {
    // удобно для групп операторов одного приоритета: +/-, */% и тд
    for (const auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

//используется для обязательных частей grammar
std::expected<Parser::Token, ParseError> Parser::consume(TokenType type, std::string message) {
    if (check(type)) {
        return advance();
    }
    return std::unexpected(make_error(peek(), std::move(message)));
}
// собирает объект ошибки
ParseError Parser::make_error(const Token& token, std::string message) const {
    return ParseError {
        .filename = filename_,
        .message = std::move(message),
        .location = token.range.begin,
    };
}

// разбирает одну верхнеуровневую декларацию
std::expected<std::unique_ptr<AST::Decl>, ParseError> Parser::parse_declaration(
    bool default_exported) {
    // Верхний уровень dispatch: смотрим на первый токен и понимаем,
    // какой именно Decl должен идти дальше.
    bool is_exported = default_exported;
    bool saw_visibility_modifier = false;
    if (match(TokenType::Export)) {
        is_exported = true;
        saw_visibility_modifier = true;
    } else if (match(TokenType::Private)) {
        is_exported = false;
        saw_visibility_modifier = true;
    }

    if (check(TokenType::Func)) {
        auto decl = parse_function_decl();
        if (!decl) {
            return std::unexpected(decl.error());
        }
        decl.value()->is_exported = is_exported;
        decl.value()->has_module_visibility = saw_visibility_modifier || default_exported;
        return std::unique_ptr<AST::Decl>(std::move(decl.value()));
    }

    if (check(TokenType::Struct)) {
        auto decl = parse_struct_decl();
        if (!decl) {
            return std::unexpected(decl.error());
        }
        decl.value()->is_exported = is_exported;
        decl.value()->has_module_visibility = saw_visibility_modifier || default_exported;
        return std::unique_ptr<AST::Decl>(std::move(decl.value()));
    }

    if (check(TokenType::Type)) {
        auto decl = parse_type_alias_decl();
        if (!decl) {
            return std::unexpected(decl.error());
        }
        decl.value()->is_exported = is_exported;
        decl.value()->has_module_visibility = saw_visibility_modifier || default_exported;
        return std::unique_ptr<AST::Decl>(std::move(decl.value()));
    }

    if (check(TokenType::Namespace)) {
        auto decl = parse_namespace_decl(default_exported);
        if (!decl) {
            return std::unexpected(decl.error());
        }
        decl.value()->is_exported = is_exported;
        decl.value()->has_module_visibility = saw_visibility_modifier || default_exported;
        return std::unique_ptr<AST::Decl>(std::move(decl.value()));
    }

    if (saw_visibility_modifier) {
        return std::unexpected(
            make_error(peek(), "expected declaration after module visibility modifier"));
    }

    return std::unexpected(make_error(peek(), "expected top-level declaration"));
}

//читает объявление функции по шагам
std::expected<std::unique_ptr<AST::FunctionDecl>, ParseError> Parser::parse_function_decl(
    bool is_method, AST::Visibility visibility) {
    auto func_token = consume(TokenType::Func, "expected 'func'");
    if (!func_token) {
        return std::unexpected(func_token.error());
    }

    std::string function_name;
    if (match(TokenType::Operator)) {
        const auto operator_token = peek();
        const auto parsed_name = operator_function_name(operator_token.type);
        if (!parsed_name.has_value()) {
            return std::unexpected(
                make_error(operator_token, "expected overloadable operator after 'operator'"));
        }
        advance();
        function_name = *parsed_name;
    } else {
        auto name_token = consume(TokenType::Identifier, "expected function name");
        if (!name_token) {
            return std::unexpected(name_token.error());
        }
        function_name = name_token->lexeme;
    }

    auto left_paren = consume(TokenType::LeftParen, "expected '(' after function name");
    if (!left_paren) {
        return std::unexpected(left_paren.error());
    }

    //разбирает параметры, return type и body функции, а потом создаёт FunctionDecl
    std::vector<AST::Parameter> parameters;
    if (!check(TokenType::RightParen)) {
        while (true) {
            auto parameter_type = parse_type();
            if (!parameter_type) {
                return std::unexpected(parameter_type.error());
            }

            auto parameter_name = consume(TokenType::Identifier, "expected parameter name");
            if (!parameter_name) {
                return std::unexpected(parameter_name.error());
            }

            AST::Parameter parameter {
                .type = std::move(parameter_type.value()),
                .name = parameter_name->lexeme,
                .default_value = nullptr,
                .range = {},
            };
            if (match(TokenType::Assign)) {
                auto default_value = parse_expression();
                if (!default_value) {
                    return std::unexpected(default_value.error());
                }
                parameter.default_value = std::move(default_value.value());
            }
            parameter.range = combine_ranges(parameter.type->range, parameter_name->range);
            parameters.push_back(std::move(parameter));

            if (!match(TokenType::Comma)) {
                break;
            }
        }
    }

    auto right_paren = consume(TokenType::RightParen, "expected ')' after parameter list");
    if (!right_paren) {
        return std::unexpected(right_paren.error());
    }

    auto arrow = consume(TokenType::Arrow, "expected '->' before function return type");
    if (!arrow) {
        return std::unexpected(arrow.error());
    }

    auto return_type = parse_type();
    if (!return_type) {
        return std::unexpected(return_type.error());
    }

    auto body = parse_block();
    if (!body) {
        return std::unexpected(body.error());
    }

    const auto range = combine_ranges(func_token->range, body.value()->range);
    return std::make_unique<AST::FunctionDecl>(
        range, function_name, std::move(parameters), std::move(return_type.value()),
        std::move(body.value()), is_method, visibility);
}

//реализация (доп A.2.3) методы и (A.2.4) видимость

// разбирает начало структуры struct, имя и {
std::expected<std::unique_ptr<AST::StructDecl>, ParseError> Parser::parse_struct_decl() {
    auto struct_token = consume(TokenType::Struct, "expected 'struct'");
    if (!struct_token) {
        return std::unexpected(struct_token.error());
    }

    auto name_token = consume(TokenType::Identifier, "expected struct name");
    if (!name_token) {
        return std::unexpected(name_token.error());
    }

    auto left_brace = consume(TokenType::LeftBrace, "expected '{' after struct name");
    if (!left_brace) {
        return std::unexpected(left_brace.error());
    }

    std::vector<AST::FieldDecl> fields;
    std::vector<std::unique_ptr<AST::FunctionDecl>> methods;
    AST::Visibility current_visibility = AST::Visibility::Public;

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        if ((check(TokenType::Public) || check(TokenType::Private)) && check_next(TokenType::Colon)) {
            current_visibility = check(TokenType::Public) ? AST::Visibility::Public
                                                          : AST::Visibility::Private;
            advance();
            advance();
            continue;
        }

        //разбор методов структуры
        if (check(TokenType::Func)) {
            auto method = parse_function_decl(true, current_visibility);
            if (!method) {
                return std::unexpected(method.error());
            }
            methods.push_back(std::move(method.value()));
            continue;
        }

        auto field_type = parse_type();
        if (!field_type) {
            return std::unexpected(field_type.error());
        }

        auto field_name = consume(TokenType::Identifier, "expected field name");
        if (!field_name) {
            return std::unexpected(field_name.error());
        }

        AST::FieldDecl field {
            .type = std::move(field_type.value()),
            .name = field_name->lexeme,
            .visibility = current_visibility,
            .range = {},
        };
        field.range = combine_ranges(field.type->range, field_name->range);
        fields.push_back(std::move(field));

        match(TokenType::Comma);
    }

    auto right_brace = consume(TokenType::RightBrace, "expected '}' after struct body");
    if (!right_brace) {
        return std::unexpected(right_brace.error());
    }

    const auto range = combine_ranges(struct_token->range, right_brace->range);
    return std::make_unique<AST::StructDecl>(range, name_token->lexeme, std::move(fields),
                                             std::move(methods));
}

//разбирает type alias
std::expected<std::unique_ptr<AST::TypeAliasDecl>, ParseError> Parser::parse_type_alias_decl() {
    auto type_token = consume(TokenType::Type, "expected 'type'");
    if (!type_token) {
        return std::unexpected(type_token.error());
    }

    auto name_token = consume(TokenType::Identifier, "expected alias name");
    if (!name_token) {
        return std::unexpected(name_token.error());
    }

    auto assign = consume(TokenType::Assign, "expected '=' after alias name");
    if (!assign) {
        return std::unexpected(assign.error());
    }

    auto target_type = parse_type();
    if (!target_type) {
        return std::unexpected(target_type.error());
    }

    auto semicolon = consume(TokenType::Semicolon, "expected ';' after type alias");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    const auto range = combine_ranges(type_token->range, semicolon->range);
    return std::make_unique<AST::TypeAliasDecl>(
        range, name_token->lexeme, std::move(target_type.value()));
}

//разбирает namespace
std::expected<std::unique_ptr<AST::NamespaceDecl>, ParseError> Parser::parse_namespace_decl(
    bool default_exported) {
    auto namespace_token = consume(TokenType::Namespace, "expected 'namespace'");
    if (!namespace_token) {
        return std::unexpected(namespace_token.error());
    }

    auto name_token = consume(TokenType::Identifier, "expected namespace name");
    if (!name_token) {
        return std::unexpected(name_token.error());
    }

    auto left_brace = consume(TokenType::LeftBrace, "expected '{' after namespace name");
    if (!left_brace) {
        return std::unexpected(left_brace.error());
    }

    std::vector<std::unique_ptr<AST::Decl>> declarations;
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        auto declaration_or_error = parse_declaration(default_exported);
        if (!declaration_or_error) {
            return std::unexpected(declaration_or_error.error());
        }
        declarations.push_back(std::move(declaration_or_error.value()));
    }

    auto right_brace = consume(TokenType::RightBrace, "expected '}' after namespace body");
    if (!right_brace) {
        return std::unexpected(right_brace.error());
    }

    const auto range = combine_ranges(namespace_token->range, right_brace->range);
    return std::make_unique<AST::NamespaceDecl>(
        range, name_token->lexeme, std::move(declarations));
}

//читает тип в любой форме
std::expected<std::unique_ptr<AST::TypeSyntax>, ParseError> Parser::parse_type() {
    std::vector<std::string> parts;
    Lexer::SourceRange range {};

    // встроенный тип (int32, bool, итд), пользовательским
    // именем/квалифицированным именем вроде Math::Point и массив
    if (is_builtin_type_token(peek().type)) {
        const auto token = advance();
        parts.push_back(token.lexeme);
        range = token.range;
    } else if (check(TokenType::Identifier)) {
        auto path_or_error = parse_identifier_path();
        if (!path_or_error) {
            return std::unexpected(path_or_error.error());
        }
        parts = std::move(path_or_error->parts);
        range = path_or_error->range;
    } else {
        return std::unexpected(make_error(peek(), "expected type"));
    }

    std::optional<std::string> array_size;
    if (match(TokenType::LeftBracket)) {
        auto size_token = consume(TokenType::IntLiteral, "expected array size");
        if (!size_token) {
            return std::unexpected(size_token.error());
        }

        auto right_bracket = consume(TokenType::RightBracket, "expected ']' after array size");
        if (!right_bracket) {
            return std::unexpected(right_bracket.error());
        }

        array_size = size_token->lexeme;
        range = combine_ranges(range, right_bracket->range);
    }

    return std::make_unique<AST::TypeSyntax>(range, std::move(parts), std::move(array_size));
}

//читает квалифицированное имя A::B::C
std::expected<Parser::ParsedPath, ParseError> Parser::parse_identifier_path() {
    auto first = consume(TokenType::Identifier, "expected identifier");
    if (!first) {
        return std::unexpected(first.error());
    }

    ParsedPath path {
        .parts = {first->lexeme},
        .range = first->range,
    };

    while (match(TokenType::ColonColon)) {
        // если встретили ::, значит читаем следующую часть квалифицированного имени
        auto part = consume(TokenType::Identifier, "expected identifier after '::'");
        if (!part) {
            return std::unexpected(part.error());
        }

        path.parts.push_back(part->lexeme);
        path.range = combine_ranges(path.range, part->range);
    }

    return path;
}

//разбирает последовательность идентификаторов через точку .
std::expected<Parser::ParsedPath, ParseError> Parser::parse_module_path() {
    auto first = consume(TokenType::Identifier, "expected module name");
    if (!first) {
        return std::unexpected(first.error());
    }

    ParsedPath path {
        .parts = {first->lexeme},
        .range = first->range,
    };

    while (match(TokenType::Dot)) {
        auto part = consume(TokenType::Identifier, "expected module name after '.'");
        if (!part) {
            return std::unexpected(part.error());
        }

        path.parts.push_back(part->lexeme);
        path.range = combine_ranges(path.range, part->range);
    }

    return path;
}

//читает { инструкции }
std::expected<std::unique_ptr<AST::BlockStmt>, ParseError> Parser::parse_block() {
    auto left_brace = consume(TokenType::LeftBrace, "expected '{'");
    if (!left_brace) {
        return std::unexpected(left_brace.error());
    }

    std::vector<std::unique_ptr<AST::Stmt>> statements;
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        auto statement_or_error = parse_statement();
        if (!statement_or_error) {
            return std::unexpected(statement_or_error.error());
        }
        statements.push_back(std::move(statement_or_error.value()));
    }

    auto right_brace = consume(TokenType::RightBrace, "expected '}' after block");
    if (!right_brace) {
        return std::unexpected(right_brace.error());
    }

    const auto range = combine_ranges(left_brace->range, right_brace->range);
    return std::make_unique<AST::BlockStmt>(range, std::move(statements));
}

std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_statement() {
    // по первому токену выбирает, какую инструкцию parser должен сейчас разобрать
    if (check(TokenType::LeftBrace)) {
        auto block = parse_block();
        if (!block) {
            return std::unexpected(block.error());
        }
        return std::unique_ptr<AST::Stmt>(std::move(block.value()));
    }

    if (check(TokenType::Let) || check(TokenType::Const)) {
        return parse_variable_decl_statement();
    }

    if (check(TokenType::If)) {
        return parse_if_statement();
    }

    if (check(TokenType::While)) {
        return parse_while_statement();
    }

    if (check(TokenType::Return)) {
        return parse_return_statement();
    }

    if (check(TokenType::Break)) {
        return parse_break_statement();
    }

    if (check(TokenType::Continue)) {
        return parse_continue_statement();
    }

    if (check(TokenType::Semicolon)) {
        return parse_empty_statement();
    }

    return parse_expression_statement(); //если ничего не подошло expression statement
}

std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_variable_decl_statement() {
    const auto keyword = advance();
    const auto mutability = keyword.type == TokenType::Let ? AST::Mutability::Mutable
                                                           : AST::Mutability::Immutable;

    // let/const Type name = expression ;
    auto type = parse_type();
    if (!type) {
        return std::unexpected(type.error());
    }

    auto name_token = consume(TokenType::Identifier, "expected variable name");
    if (!name_token) {
        return std::unexpected(name_token.error());
    }

    auto assign = consume(TokenType::Assign, "expected '=' after variable name");
    if (!assign) {
        return std::unexpected(assign.error());
    }

    auto initializer = parse_expression();
    if (!initializer) {
        return std::unexpected(initializer.error());
    }

    auto semicolon = consume(TokenType::Semicolon, "expected ';' after variable declaration");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    const auto range = combine_ranges(keyword.range, semicolon->range);
    return std::unique_ptr<AST::Stmt>(std::make_unique<AST::VariableDeclStmt>(
        range, mutability, std::move(type.value()), name_token->lexeme,
        std::move(initializer.value())));
}

// if (cond) { } else { }
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_if_statement() {
    auto if_token = consume(TokenType::If, "expected 'if'");
    if (!if_token) {
        return std::unexpected(if_token.error());
    }

    auto left_paren = consume(TokenType::LeftParen, "expected '(' after 'if'");
    if (!left_paren) {
        return std::unexpected(left_paren.error());
    }

    auto condition = parse_expression();
    if (!condition) {
        return std::unexpected(condition.error());
    }

    auto right_paren = consume(TokenType::RightParen, "expected ')' after if condition");
    if (!right_paren) {
        return std::unexpected(right_paren.error());
    }

    auto then_branch = parse_block();
    if (!then_branch) {
        return std::unexpected(then_branch.error());
    }

    std::unique_ptr<AST::BlockStmt> else_branch;
    Lexer::SourceRange range = combine_ranges(if_token->range, then_branch.value()->range);
    if (match(TokenType::Else)) {
        auto branch = parse_block();
        if (!branch) {
            return std::unexpected(branch.error());
        }
        range = combine_ranges(if_token->range, branch.value()->range);
        else_branch = std::move(branch.value());
    }

    return std::unique_ptr<AST::Stmt>(std::make_unique<AST::IfStmt>(
        range, std::move(condition.value()), std::move(then_branch.value()),
        std::move(else_branch)));
}

//while (...) {..}
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_while_statement() {
    auto while_token = consume(TokenType::While, "expected 'while'");
    if (!while_token) {
        return std::unexpected(while_token.error());
    }

    auto left_paren = consume(TokenType::LeftParen, "expected '(' after 'while'");
    if (!left_paren) {
        return std::unexpected(left_paren.error());
    }

    auto condition = parse_expression();
    if (!condition) {
        return std::unexpected(condition.error());
    }

    auto right_paren = consume(TokenType::RightParen, "expected ')' after while condition");
    if (!right_paren) {
        return std::unexpected(right_paren.error());
    }

    auto body = parse_block();
    if (!body) {
        return std::unexpected(body.error());
    }

    const auto range = combine_ranges(while_token->range, body.value()->range);
    return std::unique_ptr<AST::Stmt>(std::make_unique<AST::WhileStmt>(
        range, std::move(condition.value()), std::move(body.value())));
}

//return expr; return;
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_return_statement() {
    auto return_token = consume(TokenType::Return, "expected 'return'");
    if (!return_token) {
        return std::unexpected(return_token.error());
    }

    std::unique_ptr<AST::Expr> value;
    if (!check(TokenType::Semicolon)) {
        auto expression = parse_expression();
        if (!expression) {
            return std::unexpected(expression.error());
        }
        value = std::move(expression.value());
    }

    auto semicolon = consume(TokenType::Semicolon, "expected ';' after return statement");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    const auto range = combine_ranges(return_token->range, semicolon->range);
    return std::unique_ptr<AST::Stmt>(
        std::make_unique<AST::ReturnStmt>(range, std::move(value)));
}

//break
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_break_statement() {
    auto break_token = consume(TokenType::Break, "expected 'break'");
    if (!break_token) {
        return std::unexpected(break_token.error());
    }

    auto semicolon = consume(TokenType::Semicolon, "expected ';' after break");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    return std::unique_ptr<AST::Stmt>(
        std::make_unique<AST::BreakStmt>(combine_ranges(break_token->range, semicolon->range)));
}

//continue
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_continue_statement() {
    auto continue_token = consume(TokenType::Continue, "expected 'continue'");
    if (!continue_token) {
        return std::unexpected(continue_token.error());
    }

    auto semicolon = consume(TokenType::Semicolon, "expected ';' after continue");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    return std::unique_ptr<AST::Stmt>(std::make_unique<AST::ContinueStmt>(
        combine_ranges(continue_token->range, semicolon->range)));
}

//просто выражение с ; (print("hello");)
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_expression_statement() {
    auto expression = parse_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    auto semicolon = consume(TokenType::Semicolon, "expected ';' after expression");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    const auto range = combine_ranges(expression.value()->range, semicolon->range);
    return std::unique_ptr<AST::Stmt>(
        std::make_unique<AST::ExprStmt>(range, std::move(expression.value())));
}

// ;
std::expected<std::unique_ptr<AST::Stmt>, ParseError> Parser::parse_empty_statement() {
    auto semicolon = consume(TokenType::Semicolon, "expected ';'");
    if (!semicolon) {
        return std::unexpected(semicolon.error());
    }

    return std::unique_ptr<AST::Stmt>(std::make_unique<AST::EmptyStmt>(semicolon->range));
}

//верхняя точка входа для разбора выражений
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_expression() {
    return parse_assignment_expression();
}

//присваивание 
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_assignment_expression() {
    auto left = parse_pipe_expression();
    if (!left) {
        return std::unexpected(left.error());
    }

    // присваивание правоассоциативно
    if (!match(TokenType::Assign)) {
        return left;
    }

    const auto assign_token = previous();
    if (!is_assignable_expression(*left.value())) {
        return std::unexpected(make_error(assign_token, "invalid assignment target"));
    }

    auto right = parse_assignment_expression();
    if (!right) {
        return std::unexpected(right.error());
    }

    const auto range = combine_ranges(left.value()->range, right.value()->range);
    return std::unique_ptr<AST::Expr>(std::make_unique<AST::AssignmentExpr>(
        range, std::move(left.value()), std::move(right.value())));
}

// x |> f (доп A.1.9)
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_pipe_expression() {
    auto expression = parse_logical_or_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match(TokenType::PipeGreater)) {
        auto right = parse_logical_or_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = lower_pipe_expression(std::move(expression.value()),
                                           std::move(right.value()), range);
    }

    return expression;
}

// ||
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_logical_or_expression() {
    auto expression = parse_logical_and_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match(TokenType::PipePipe)) {
        const auto op = previous();
        auto right = parse_logical_and_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}//&&
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_logical_and_expression() {
    auto expression = parse_bitwise_or_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match(TokenType::AmpAmp)) {
        const auto op = previous();
        auto right = parse_bitwise_or_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//|
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_bitwise_or_expression() {
    auto expression = parse_bitwise_xor_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match(TokenType::Pipe)) {
        const auto op = previous();
        auto right = parse_bitwise_xor_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//^
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_bitwise_xor_expression() {
    auto expression = parse_bitwise_and_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match(TokenType::Caret)) {
        const auto op = previous();
        auto right = parse_bitwise_and_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//&
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_bitwise_and_expression() {
    auto expression = parse_equality_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match(TokenType::Amp)) {
        const auto op = previous();
        auto right = parse_equality_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//== , !=
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_equality_expression() {
    auto expression = parse_relational_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match_any({TokenType::EqualEqual, TokenType::BangEqual})) {
        const auto op = previous();
        auto right = parse_relational_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}
//<,>,<=,>=
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_relational_expression() {
    auto expression = parse_shift_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match_any({TokenType::Less, TokenType::Greater, TokenType::LessEqual,
                      TokenType::GreaterEqual})) {
        const auto op = previous();
        auto right = parse_shift_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//<<,>>
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_shift_expression() {
    auto expression = parse_additive_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match_any({TokenType::ShiftLeft, TokenType::ShiftRight})) {
        const auto op = previous();
        auto right = parse_additive_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//+/-
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_additive_expression() {
    auto expression = parse_multiplicative_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match_any({TokenType::Plus, TokenType::Minus})) {
        const auto op = previous();
        auto right = parse_multiplicative_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//*, /, %
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_multiplicative_expression() {
    auto expression = parse_unary_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    while (match_any({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
        const auto op = previous();
        auto right = parse_unary_expression();
        if (!right) {
            return std::unexpected(right.error());
        }

        const auto range = combine_ranges(expression.value()->range, right.value()->range);
        expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::BinaryExpr>(
            range, op.type, op.lexeme, std::move(expression.value()), std::move(right.value())));
    }

    return expression;
}

//унарные
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_unary_expression() {
    if (match_any({TokenType::Minus, TokenType::Bang, TokenType::Tilde})) {
        const auto op = previous();
        auto operand = parse_unary_expression();
        if (!operand) {
            return std::unexpected(operand.error());
        }

        const auto range = combine_ranges(op.range, operand.value()->range);
        return std::unique_ptr<AST::Expr>(std::make_unique<AST::UnaryExpr>(
            range, op.type, op.lexeme, std::move(operand.value())));
    }

    return parse_cast_expression();
}

//явное привидение типа cast<int32>(..)
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_cast_expression() {
    if (!match(TokenType::Cast)) {
        return parse_postfix_expression();
    }

    const auto cast_token = previous();

    auto less = consume(TokenType::Less, "expected '<' after 'cast'");
    if (!less) {
        return std::unexpected(less.error());
    }

    auto target_type = parse_type();
    if (!target_type) {
        return std::unexpected(target_type.error());
    }

    auto greater = consume(TokenType::Greater, "expected '>' after cast type");
    if (!greater) {
        return std::unexpected(greater.error());
    }

    auto left_paren = consume(TokenType::LeftParen, "expected '(' before cast operand");
    if (!left_paren) {
        return std::unexpected(left_paren.error());
    }

    auto expression = parse_expression();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    auto right_paren = consume(TokenType::RightParen, "expected ')' after cast operand");
    if (!right_paren) {
        return std::unexpected(right_paren.error());
    }

    const auto range = combine_ranges(cast_token.range, right_paren->range);
    return std::unique_ptr<AST::Expr>(std::make_unique<AST::CastExpr>(
        range, std::move(target_type.value()), std::move(expression.value())));
}

//postfix-суффиксы
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_postfix_expression() {
    auto expression = parse_atom();
    if (!expression) {
        return std::unexpected(expression.error());
    }

    // рostfix-операции можно наращивать цепочкой: call, index, field access
    // поэтому после atom цикл продолжается, пока встречаются соответствующие суффиксы
    while (true) {
        if (match(TokenType::LeftParen)) {
            std::vector<AST::CallArgument> arguments;
            if (!check(TokenType::RightParen)) {
                while (true) {
                    std::optional<std::string> name;
                    Lexer::SourceRange argument_range {};

                    if (check(TokenType::Identifier) && check_next(TokenType::Assign)) {
                        const auto named_token = advance();
                        name = named_token.lexeme;
                        argument_range = named_token.range;
                        advance();
                    }

                    auto argument = parse_expression();
                    if (!argument) {
                        return std::unexpected(argument.error());
                    }

                    if (!name.has_value()) {
                        argument_range = argument.value()->range;
                    } else {
                        argument_range = combine_ranges(argument_range, argument.value()->range);
                    }

                    arguments.push_back(AST::CallArgument {
                        .name = std::move(name),
                        .value = std::move(argument.value()),
                        .range = argument_range,
                    });

                    if (!match(TokenType::Comma)) {
                        break;
                    }
                }
            }

            auto right_paren = consume(TokenType::RightParen, "expected ')' after argument list");
            if (!right_paren) {
                return std::unexpected(right_paren.error());
            }

            const auto range = combine_ranges(expression.value()->range, right_paren->range);
            expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::CallExpr>(
                range, std::move(expression.value()), std::move(arguments)));
            continue;
        }

        if (match(TokenType::LeftBracket)) {
            auto index = parse_expression();
            if (!index) {
                return std::unexpected(index.error());
            }

            auto right_bracket = consume(TokenType::RightBracket, "expected ']' after index");
            if (!right_bracket) {
                return std::unexpected(right_bracket.error());
            }

            const auto range = combine_ranges(expression.value()->range, right_bracket->range);
            expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::IndexExpr>(
                range, std::move(expression.value()), std::move(index.value())));
            continue;
        }

        if (match(TokenType::Dot)) {
            auto field = consume(TokenType::Identifier, "expected field name after '.'");
            if (!field) {
                return std::unexpected(field.error());
            }

            const auto range = combine_ranges(expression.value()->range, field->range);
            expression = std::unique_ptr<AST::Expr>(std::make_unique<AST::FieldAccessExpr>(
                range, std::move(expression.value()), field->lexeme));
            continue;
        }

        break;
    }

    return expression;
}

//(доп A.2.1) читает if (cond) then_expr else else_expr
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_if_expression() {
    auto if_token = consume(TokenType::If, "expected 'if'");
    if (!if_token) {
        return std::unexpected(if_token.error());
    }

    auto left_paren = consume(TokenType::LeftParen, "expected '(' after 'if'");
    if (!left_paren) {
        return std::unexpected(left_paren.error());
    }

    auto condition = parse_expression();
    if (!condition) {
        return std::unexpected(condition.error());
    }

    auto right_paren = consume(TokenType::RightParen, "expected ')' after if condition");
    if (!right_paren) {
        return std::unexpected(right_paren.error());
    }

    auto then_branch = parse_expression();
    if (!then_branch) {
        return std::unexpected(then_branch.error());
    }

    auto else_token = consume(TokenType::Else, "expected 'else' in if expression");
    if (!else_token) {
        return std::unexpected(else_token.error());
    }

    auto else_branch = parse_expression();
    if (!else_branch) {
        return std::unexpected(else_branch.error());
    }

    const auto range = combine_ranges(if_token->range, else_branch.value()->range);
    return std::unique_ptr<AST::Expr>(std::make_unique<AST::IfExpr>(
        range, std::move(condition.value()), std::move(then_branch.value()),
        std::move(else_branch.value())));
}
//атомарные выражения
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_atom() {
    //  это самый нижний уровень выражений литералы, идентификаторы, скобки и составные литералы
    if (check(TokenType::If)) {
        return parse_if_expression();
    }

    if (match(TokenType::IntLiteral)) {
        const auto token = previous();
        return std::unique_ptr<AST::Expr>(
            std::make_unique<AST::IntLiteralExpr>(token.range, token.lexeme));
    }

    if (match(TokenType::FloatLiteral)) {
        const auto token = previous();
        return std::unique_ptr<AST::Expr>(
            std::make_unique<AST::FloatLiteralExpr>(token.range, token.lexeme));
    }

    if (match(TokenType::StringLiteral)) {
        const auto token = previous();
        return std::unique_ptr<AST::Expr>(
            std::make_unique<AST::StringLiteralExpr>(token.range, token.lexeme));
    }

    if (match(TokenType::CharLiteral)) {
        const auto token = previous();
        return std::unique_ptr<AST::Expr>(
            std::make_unique<AST::CharLiteralExpr>(token.range, token.lexeme));
    }

    if (match(TokenType::True)) {
        const auto token = previous();
        return std::unique_ptr<AST::Expr>(std::make_unique<AST::BoolLiteralExpr>(token.range, true));
    }

    if (match(TokenType::False)) {
        const auto token = previous();
        return std::unique_ptr<AST::Expr>(
            std::make_unique<AST::BoolLiteralExpr>(token.range, false));
    }

    if (check(TokenType::Identifier)) {
        auto path_or_error = parse_identifier_path();
        if (!path_or_error) {
            return std::unexpected(path_or_error.error());
        }

        if (check(TokenType::LeftBrace)) {
            return parse_struct_literal(std::move(path_or_error.value()));
        }

        if (path_or_error->parts.size() == 1) {
            return std::unique_ptr<AST::Expr>(std::make_unique<AST::IdentifierExpr>(
                path_or_error->range, path_or_error->parts.front()));
        }

        return std::unique_ptr<AST::Expr>(std::make_unique<AST::NamespaceAccessExpr>(
            path_or_error->range, std::move(path_or_error->parts)));
    }

    if (match(TokenType::LeftParen)) {
        auto expression = parse_expression();
        if (!expression) {
            return std::unexpected(expression.error());
        }

        auto right_paren = consume(TokenType::RightParen, "expected ')' after expression");
        if (!right_paren) {
            return std::unexpected(right_paren.error());
        }

        return expression;
    }

    if (check(TokenType::LeftBracket)) {
        return parse_array_literal();
    }

    return std::unexpected(make_error(peek(), "expected expression"));
}

//литерал массива
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_array_literal() {
    auto left_bracket = consume(TokenType::LeftBracket, "expected '['");
    if (!left_bracket) {
        return std::unexpected(left_bracket.error());
    }

    std::vector<std::unique_ptr<AST::Expr>> elements;
    if (!check(TokenType::RightBracket)) {
        while (true) {
            auto element = parse_expression();
            if (!element) {
                return std::unexpected(element.error());
            }
            elements.push_back(std::move(element.value()));

            if (!match(TokenType::Comma)) {
                break;
            }

            if (check(TokenType::RightBracket)) {
                break;
            }
        }
    }

    auto right_bracket = consume(TokenType::RightBracket, "expected ']' after array literal");
    if (!right_bracket) {
        return std::unexpected(right_bracket.error());
    }

    const auto range = combine_ranges(left_bracket->range, right_bracket->range);
    return std::unique_ptr<AST::Expr>(
        std::make_unique<AST::ArrayLiteralExpr>(range, std::move(elements)));
}

//литерал структуры Point { x: 10, y: 20 }
std::expected<std::unique_ptr<AST::Expr>, ParseError> Parser::parse_struct_literal(
    ParsedPath type_path) {
    auto left_brace = consume(TokenType::LeftBrace, "expected '{' after struct type");
    if (!left_brace) {
        return std::unexpected(left_brace.error());
    }

    std::vector<AST::FieldInitializer> fields;
    if (!check(TokenType::RightBrace)) {
        while (true) {
            auto field_name = consume(TokenType::Identifier, "expected field name");
            if (!field_name) {
                return std::unexpected(field_name.error());
            }

            auto colon = consume(TokenType::Colon, "expected ':' after field name");
            if (!colon) {
                return std::unexpected(colon.error());
            }

            auto value = parse_expression();
            if (!value) {
                return std::unexpected(value.error());
            }

            AST::FieldInitializer field {
                .name = field_name->lexeme,
                .value = std::move(value.value()),
                .range = {},
            };
            field.range = combine_ranges(field_name->range, field.value->range);
            fields.push_back(std::move(field));

            if (!match(TokenType::Comma)) {
                break;
            }

            if (check(TokenType::RightBrace)) {
                break;
            }
        }
    }

    auto right_brace = consume(TokenType::RightBrace, "expected '}' after struct literal");
    if (!right_brace) {
        return std::unexpected(right_brace.error());
    }

    const auto range = combine_ranges(type_path.range, right_brace->range);
    return std::unique_ptr<AST::Expr>(std::make_unique<AST::StructLiteralExpr>(
        range, std::move(type_path.parts), std::move(fields)));
}

//может ли текущий токен быть началом типа
bool Parser::is_type_start() const {
    return is_builtin_type_token(peek().type) || check(TokenType::Identifier);
}

//перечисляет все встроенные типы 
bool Parser::is_builtin_type_token(TokenType type) const {
    switch (type) {
        case TokenType::Int8:
        case TokenType::Int16:
        case TokenType::Int32:
        case TokenType::Int64:
        case TokenType::UInt8:
        case TokenType::UInt16:
        case TokenType::UInt32:
        case TokenType::UInt64:
        case TokenType::Float32:
        case TokenType::Float64:
        case TokenType::Bool:
        case TokenType::Char:
        case TokenType::String:
        case TokenType::Void:
            return true;
        default:
            return false;
    }
}
 
//можно ли это выражение ставить слева от =
bool Parser::is_assignable_expression(const AST::Expr& expr) const {
    return dynamic_cast<const AST::IdentifierExpr*>(&expr) != nullptr ||
           dynamic_cast<const AST::NamespaceAccessExpr*>(&expr) != nullptr ||
           dynamic_cast<const AST::IndexExpr*>(&expr) != nullptr ||
           dynamic_cast<const AST::FieldAccessExpr*>(&expr) != nullptr;
}

} 
