#include "semantic.hpp"

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

bool is_builtin_type_name(std::string_view name) {
    return name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
           name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64" ||
           name == "float32" || name == "float64" || name == "bool" || name == "string" ||
           name == "void";
}

bool is_numeric_literal_expr(const AST::Expr& expr) {
    return dynamic_cast<const AST::IntLiteralExpr*>(&expr) != nullptr ||
           dynamic_cast<const AST::FloatLiteralExpr*>(&expr) != nullptr;
}

}  

namespace Semantic {

std::string format_error(const SemanticError& error) {
    std::ostringstream stream;
    stream << error.filename << ':' << error.location.line << ':' << error.location.column
           << ": error: " << error.message;
    return stream.str();
}

class Analyzer {
  private:
    struct Type;
    using TypePtr = std::shared_ptr<Type>;
    struct Type {
        std::string name {};

        [[nodiscard]] bool is_void() const { return name == "void"; }
        [[nodiscard]] bool is_bool() const { return name == "bool"; }
        [[nodiscard]] bool is_string() const { return name == "string"; }
        [[nodiscard]] bool is_integer() const {
            return name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
                   name == "uint8" || name == "uint16" || name == "uint32" ||
                   name == "uint64";
        }
        [[nodiscard]] bool is_float() const {
            return name == "float32" || name == "float64";
        }
        [[nodiscard]] bool is_numeric() const { return is_integer() || is_float(); }
    };

    enum class BuiltinKind {
        None,
        Print,
        Input,
        Len,
        Exit,
        Panic,
    };

    struct FunctionSymbol {
        std::string full_name {};
        const AST::FunctionDecl* decl = nullptr;
        std::vector<TypePtr> parameter_types;
        TypePtr return_type {};
        bool is_builtin = false;
        BuiltinKind builtin_kind = BuiltinKind::None;
    };

    struct VariableSymbol {
        TypePtr type {};
        AST::Mutability mutability = AST::Mutability::Immutable;
    };

    struct ExprInfo {
        TypePtr type {};
        const FunctionSymbol* function = nullptr;
        bool is_lvalue = false;
        bool is_mutable_lvalue = false;
    };

  public:
    explicit Analyzer(std::string filename) : filename_(std::move(filename)) { init_builtins(); }

    std::expected<SemanticResult, SemanticError> analyze(const AST::Program& program) {
        result_.program = &program;

        //собираем и регистрируем только функции верхнего уровня
        auto collected = collect_functions(program.declarations);
        if (!collected) {
            return std::unexpected(collected.error());
        }

        //отдельно проверяем обязательную точку входа main()
        auto main_checked = check_main_signature();
        if (!main_checked) {
            return std::unexpected(main_checked.error());
        }

    //проверяем тела функций
        for (const auto& [_, function] : functions_) {
            if (function->is_builtin) {
                continue;
            }

            auto analyzed = analyze_function(*function);
            if (!analyzed) {
                return std::unexpected(analyzed.error());
            }
        }

        return result_;
    }

  private:
    SemanticResult result_ {};
    std::string filename_;
    std::unordered_map<std::string, TypePtr> builtin_types_;
    std::unordered_map<std::string, std::unique_ptr<FunctionSymbol>> functions_;
    std::vector<std::unordered_map<std::string, VariableSymbol>> local_scopes_;
    TypePtr current_return_type_ {};
    int loop_depth_ = 0;

    void init_builtins() {
        //cначала регистрируем все встроенные типы языка
        register_builtin_type("int8");
        register_builtin_type("int16");
        register_builtin_type("int32");
        register_builtin_type("int64");
        register_builtin_type("uint8");
        register_builtin_type("uint16");
        register_builtin_type("uint32");
        register_builtin_type("uint64");
        register_builtin_type("float32");
        register_builtin_type("float64");
        register_builtin_type("bool");
        register_builtin_type("string");
        register_builtin_type("void");

//затем добавляем встроенные функции с уже известными сигнатурами
        register_builtin_function("print", BuiltinKind::Print, builtin_type("void"));
        register_builtin_function("input", BuiltinKind::Input, builtin_type("string"));
        register_builtin_function("len", BuiltinKind::Len, builtin_type("int32"));
        register_builtin_function("exit", BuiltinKind::Exit, builtin_type("void"));
        register_builtin_function("panic", BuiltinKind::Panic, builtin_type("void"));
    }

    void register_builtin_type(std::string name) {
        builtin_types_.emplace(name, std::make_shared<Type>(Type {.name = std::move(name)}));
    }

    void register_builtin_function(std::string name, BuiltinKind kind, TypePtr return_type) {
        auto symbol = std::make_unique<FunctionSymbol>();
        symbol->full_name = name;
        symbol->return_type = std::move(return_type);
        symbol->is_builtin = true;
        symbol->builtin_kind = kind;
        functions_.emplace(symbol->full_name, std::move(symbol));
    }

    TypePtr builtin_type(const std::string& name) const { return builtin_types_.at(name); }

    [[nodiscard]] bool same_type(const TypePtr& left, const TypePtr& right) const {
        return left.get() == right.get();
    }

    [[nodiscard]] bool is_printable_type(const TypePtr& type) const {
        return type != nullptr && (type->is_numeric() || type->is_bool() || type->is_string());
    }

    [[nodiscard]] SemanticType to_public_type(const TypePtr& type) const {
        SemanticType result;
        if (type != nullptr) {
            result.kind = SemanticTypeKind::Builtin;
            result.name = type->name;
        }
        return result;
    }

    void annotate_expr(const AST::Expr& expression, const TypePtr& type) {
        if (type != nullptr) {
            result_.expr_types[&expression] = to_public_type(type);
        }
    }

    ExprInfo make_value_info(const AST::Expr& expression, const TypePtr& type,
                             bool is_lvalue = false, bool is_mutable_lvalue = false) {
        annotate_expr(expression, type);
        return ExprInfo {
            .type = type,
            .function = nullptr,
            .is_lvalue = is_lvalue,
            .is_mutable_lvalue = is_mutable_lvalue,
        };
    }

    ExprInfo make_function_info(const AST::Expr& expression, const FunctionSymbol* function) {
        result_.resolved_functions[&expression] = function->full_name;
        return ExprInfo {
            .type = nullptr,
            .function = function,
            .is_lvalue = false,
            .is_mutable_lvalue = false,
        };
    }

    [[nodiscard]] SemanticError make_error(const Lexer::SourceLocation& location,
                                           std::string message) const {
        return SemanticError {
            .filename = filename_,
            .message = std::move(message),
            .location = location,
        };
    }

    [[nodiscard]] SemanticError make_error(const Lexer::SourceRange& range,
                                           std::string message) const {
        return make_error(range.begin, std::move(message));
    }

    std::expected<void, SemanticError> collect_functions(
        const std::vector<std::unique_ptr<AST::Decl>>& declarations) {
        for (const auto& declaration : declarations) {
            if (const auto* function = dynamic_cast<const AST::FunctionDecl*>(declaration.get())) {
                if (functions_.contains(function->name)) {
                    return std::unexpected(make_error(
                        function->range, "duplicate function '" + function->name + '\''));
                }

                auto symbol = std::make_unique<FunctionSymbol>();
                symbol->full_name = function->name;
                symbol->decl = function;

                for (const auto& parameter : function->parameters) {
                    auto type = resolve_builtin_type(*parameter.type, false);
                    if (!type) {
                        return std::unexpected(type.error());
                    }
                    symbol->parameter_types.push_back(*type);
                }

                auto return_type = resolve_builtin_type(*function->return_type, true);
                if (!return_type) {
                    return std::unexpected(return_type.error());
                }
                symbol->return_type = *return_type;

                FunctionInfo info;
                info.full_name = function->name;
                info.return_type = to_public_type(symbol->return_type);
                for (const auto& parameter_type : symbol->parameter_types) {
                    info.parameter_types.push_back(to_public_type(parameter_type));
                }
                result_.functions[function] = info;

                functions_.emplace(symbol->full_name, std::move(symbol));
                continue;
            }

            if (dynamic_cast<const AST::StructDecl*>(declaration.get()) != nullptr) {
                return std::unexpected(make_error(
                    declaration->range, "TODO: struct declarations are not implemented in this stage"));
            }

            if (dynamic_cast<const AST::TypeAliasDecl*>(declaration.get()) != nullptr) {
                return std::unexpected(make_error(
                    declaration->range, "TODO: type aliases are not implemented in this stage"));
            }

            if (dynamic_cast<const AST::NamespaceDecl*>(declaration.get()) != nullptr) {
                return std::unexpected(make_error(
                    declaration->range, "TODO: namespaces are not implemented in this stage"));
            }
        }

        return {};
    }

    std::expected<void, SemanticError> check_main_signature() {
        const auto it = functions_.find("main");
        if (it == functions_.end() || it->second->is_builtin) {
            return std::unexpected(make_error(Lexer::SourceLocation {},
                                              "function 'main' with signature 'func main() -> int32' is required"));
        }

        if (!it->second->parameter_types.empty() ||
            !same_type(it->second->return_type, builtin_type("int32"))) {
            return std::unexpected(make_error(it->second->decl->range.begin,
                                              "function 'main' must have signature 'func main() -> int32'"));
        }

        return {};
    }

    std::expected<TypePtr, SemanticError> resolve_builtin_type(const AST::TypeSyntax& syntax,
                                                               bool allow_void) {
        if (syntax.array_size.has_value()) {
            return std::unexpected(
                make_error(syntax.range, "TODO: array types are not implemented in this stage"));
        }

        //здесь разрешаем только встроенные типы без namespace, alias и struct
        if (syntax.name_parts.size() != 1 || !is_builtin_type_name(syntax.name_parts.front())) {
            return std::unexpected(make_error(
                syntax.range, "TODO: only builtin types are supported in this stage"));
        }

        const auto type = builtin_type(syntax.name_parts.front());
        if (!allow_void && type->is_void()) {
            return std::unexpected(make_error(syntax.range, "type 'void' is not allowed here"));
        }

        return type;
    }

    void push_scope() { local_scopes_.push_back({}); }
    void pop_scope() { local_scopes_.pop_back(); }

    std::expected<void, SemanticError> declare_local(const std::string& name,
                                                     VariableSymbol symbol,
                                                     const Lexer::SourceRange& range) {
        auto& scope = local_scopes_.back();
        if (scope.contains(name)) {
            return std::unexpected(make_error(
                range, "duplicate declaration of local name '" + name + '\''));
        }

        scope.emplace(name, std::move(symbol));
        return {};
    }

    const VariableSymbol* lookup_local(const std::string& name) const {
        for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
            const auto found = it->find(name);
            if (found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

    std::expected<void, SemanticError> analyze_function(const FunctionSymbol& function) {
        const auto saved_return_type = current_return_type_;
        const auto saved_loop_depth = loop_depth_;
        current_return_type_ = function.return_type;
        loop_depth_ = 0;
        local_scopes_.clear();
        push_scope();

        std::unordered_set<std::string> parameter_names;
        for (std::size_t i = 0; i < function.decl->parameters.size(); ++i) {
            const auto& parameter = function.decl->parameters[i];
            if (!parameter_names.insert(parameter.name).second) {
                restore_function_state(saved_return_type, saved_loop_depth);
                return std::unexpected(
                    make_error(parameter.range, "duplicate parameter '" + parameter.name + '\''));
            }

            auto declared = declare_local(
                parameter.name,
                VariableSymbol {
                    .type = function.parameter_types[i],
                    .mutability = AST::Mutability::Immutable,
                },
                parameter.range);
            if (!declared) {
                restore_function_state(saved_return_type, saved_loop_depth);
                return std::unexpected(declared.error());
            }
        }

        auto returns = analyze_block(*function.decl->body, false);
        restore_function_state(saved_return_type, saved_loop_depth);
        if (!returns) {
            return std::unexpected(returns.error());
        }

        if (!function.return_type->is_void() && !*returns) {
            return std::unexpected(make_error(function.decl->body->range,
                                              "not all control paths return a value"));
        }

        return {};
    }

    void restore_function_state(const TypePtr& saved_return_type, int saved_loop_depth) {
        local_scopes_.clear();
        current_return_type_ = saved_return_type;
        loop_depth_ = saved_loop_depth;
    }

    std::expected<bool, SemanticError> analyze_block(const AST::BlockStmt& block, bool create_scope) {
        if (create_scope) {
            push_scope();
        }

        bool guarantees_return = false;
        for (const auto& statement : block.statements) {
            if (guarantees_return) {
                break;
            }

            auto statement_returns = analyze_statement(*statement);
            if (!statement_returns) {
                if (create_scope) {
                    pop_scope();
                }
                return std::unexpected(statement_returns.error());
            }
            guarantees_return = *statement_returns;
        }

        if (create_scope) {
            pop_scope();
        }
        return guarantees_return;
    }

    std::expected<bool, SemanticError> analyze_statement(const AST::Stmt& statement) {
        if (const auto* block = dynamic_cast<const AST::BlockStmt*>(&statement)) {
            return analyze_block(*block, true);
        }

        if (const auto* declaration = dynamic_cast<const AST::VariableDeclStmt*>(&statement)) {
            auto variable_type = resolve_builtin_type(*declaration->type, false);
            if (!variable_type) {
                return std::unexpected(variable_type.error());
            }

            auto initializer =
                analyze_value_expression(*declaration->initializer, *variable_type, "initializer");
            if (!initializer) {
                return std::unexpected(initializer.error());
            }

            if (!same_type(initializer->type, *variable_type)) {
                return std::unexpected(make_error(
                    declaration->initializer->range,
                    "initializer type '" + initializer->type->name +
                        "' does not match variable type '" + (*variable_type)->name + '\''));
            }

            auto declared = declare_local(
                declaration->name,
                VariableSymbol {
                    .type = *variable_type,
                    .mutability = declaration->mutability,
                },
                declaration->range);
            if (!declared) {
                return std::unexpected(declared.error());
            }

            result_.variables[declaration] = VariableInfo {
                .type = to_public_type(*variable_type),
                .is_mutable = declaration->mutability == AST::Mutability::Mutable,
            };
            return false;
        }

        if (const auto* if_stmt = dynamic_cast<const AST::IfStmt*>(&statement)) {
            auto condition =
                analyze_value_expression(*if_stmt->condition, builtin_type("bool"), "condition");
            if (!condition) {
                return std::unexpected(condition.error());
            }
            if (!same_type(condition->type, builtin_type("bool"))) {
                return std::unexpected(
                    make_error(if_stmt->condition->range, "if condition must have type 'bool'"));
            }

            auto then_returns = analyze_block(*if_stmt->then_branch, true);
            if (!then_returns) {
                return std::unexpected(then_returns.error());
            }

            bool else_returns = false;
            if (if_stmt->else_branch) {
                auto branch_returns = analyze_block(*if_stmt->else_branch, true);
                if (!branch_returns) {
                    return std::unexpected(branch_returns.error());
                }
                else_returns = *branch_returns;
            }

            return *then_returns && if_stmt->else_branch != nullptr && else_returns;
        }

        if (const auto* while_stmt = dynamic_cast<const AST::WhileStmt*>(&statement)) {
            auto condition =
                analyze_value_expression(*while_stmt->condition, builtin_type("bool"), "condition");
            if (!condition) {
                return std::unexpected(condition.error());
            }
            if (!same_type(condition->type, builtin_type("bool"))) {
                return std::unexpected(make_error(while_stmt->condition->range,
                                                  "while condition must have type 'bool'"));
            }

            ++loop_depth_;
            auto body_ok = analyze_block(*while_stmt->body, true);
            --loop_depth_;
            if (!body_ok) {
                return std::unexpected(body_ok.error());
            }
            return false;
        }

        if (const auto* return_stmt = dynamic_cast<const AST::ReturnStmt*>(&statement)) {
            if (current_return_type_->is_void()) {
                if (return_stmt->value) {
                    return std::unexpected(
                        make_error(return_stmt->value->range, "void function cannot return a value"));
                }
                return true;
            }

            if (!return_stmt->value) {
                return std::unexpected(make_error(statement.range,
                                                  "non-void function must return a value"));
            }

            auto value =
                analyze_value_expression(*return_stmt->value, current_return_type_, "return value");
            if (!value) {
                return std::unexpected(value.error());
            }

            if (!same_type(value->type, current_return_type_)) {
                return std::unexpected(make_error(
                    return_stmt->value->range,
                    "return type '" + value->type->name +
                        "' does not match function return type '" +
                        current_return_type_->name + '\''));
            }
            return true;
        }

        if (dynamic_cast<const AST::BreakStmt*>(&statement) != nullptr) {
            if (loop_depth_ <= 0) {
                return std::unexpected(make_error(statement.range, "'break' is only valid inside a loop"));
            }
            return false;
        }

        if (dynamic_cast<const AST::ContinueStmt*>(&statement) != nullptr) {
            if (loop_depth_ <= 0) {
                return std::unexpected(make_error(statement.range, "'continue' is only valid inside a loop"));
            }
            return false;
        }

        if (const auto* expression_stmt = dynamic_cast<const AST::ExprStmt*>(&statement)) {
            auto expression = analyze_expression(*expression_stmt->expression);
            if (!expression) {
                return std::unexpected(expression.error());
            }

            if (expression->function != nullptr && expression->type == nullptr) {
                return std::unexpected(make_error(
                    expression_stmt->expression->range,
                    "function name must be used in a call expression"));
            }
            return false;
        }

        if (dynamic_cast<const AST::EmptyStmt*>(&statement) != nullptr) {
            return false;
        }

        return std::unexpected(make_error(statement.range, "unsupported statement kind"));
    }

    std::expected<ExprInfo, SemanticError> analyze_value_expression(const AST::Expr& expression,
                                                                    TypePtr expected_type,
                                                                    const std::string& context) {
        auto info = analyze_expression(expression, std::move(expected_type));
        if (!info) {
            return std::unexpected(info.error());
        }

        if (info->type == nullptr) {
            if (info->function != nullptr) {
                return std::unexpected(make_error(expression.range,
                                                  "function name cannot be used as " + context));
            }
            return std::unexpected(make_error(expression.range, "expected a value expression"));
        }
        return info;
    }

    std::expected<ExprInfo, SemanticError> analyze_expression(const AST::Expr& expression,
                                                              TypePtr expected_type = nullptr) {
        if (const auto* identifier = dynamic_cast<const AST::IdentifierExpr*>(&expression)) {
            //идентификатор может оказаться либо локальной переменной, либоименем функции.эти два случая различаются уже на семантике
            if (const auto* local = lookup_local(identifier->name)) {
                return make_value_info(expression, local->type, true,
                                       local->mutability == AST::Mutability::Mutable);
            }

            if (functions_.contains(identifier->name)) {
                return make_function_info(expression, functions_.at(identifier->name).get());
            }

            return std::unexpected(
                make_error(expression.range, "unknown identifier '" + identifier->name + '\''));
        }

        if (dynamic_cast<const AST::NamespaceAccessExpr*>(&expression) != nullptr) {
            return std::unexpected(make_error(
                expression.range, "TODO: namespaces are not implemented in this stage"));
        }

        if (dynamic_cast<const AST::BoolLiteralExpr*>(&expression) != nullptr) {
            return make_value_info(expression, builtin_type("bool"));
        }

        if (dynamic_cast<const AST::StringLiteralExpr*>(&expression) != nullptr) {
            return make_value_info(expression, builtin_type("string"));
        }

        if (dynamic_cast<const AST::IntLiteralExpr*>(&expression) != nullptr) {
            if (expected_type != nullptr && expected_type->is_integer()) {
                return make_value_info(expression, expected_type);
            }
            return make_value_info(expression, builtin_type("int32"));
        }

        if (dynamic_cast<const AST::FloatLiteralExpr*>(&expression) != nullptr) {
            if (expected_type != nullptr && expected_type->is_float()) {
                return make_value_info(expression, expected_type);
            }
            return make_value_info(expression, builtin_type("float64"));
        }

        if (const auto* unary = dynamic_cast<const AST::UnaryExpr*>(&expression)) {
            auto operand = analyze_value_expression(*unary->operand, nullptr, "operand");
            if (!operand) {
                return std::unexpected(operand.error());
            }

            if (unary->op_type == Lexer::TokenType::Minus) {
                if (!operand->type->is_numeric()) {
                    return std::unexpected(
                        make_error(expression.range, "unary '-' requires a numeric operand"));
                }
                return make_value_info(expression, operand->type);
            }

            if (unary->op_type == Lexer::TokenType::Bang) {
                if (!same_type(operand->type, builtin_type("bool"))) {
                    return std::unexpected(
                        make_error(expression.range, "'!' requires an operand of type 'bool'"));
                }
                return make_value_info(expression, builtin_type("bool"));
            }

            return std::unexpected(make_error(expression.range, "unsupported unary operator"));
        }

        if (const auto* binary = dynamic_cast<const AST::BinaryExpr*>(&expression)) {
            return analyze_binary_expression(*binary);
        }

        if (const auto* assignment = dynamic_cast<const AST::AssignmentExpr*>(&expression)) {
            auto target = analyze_assignment_target(*assignment->target);
            if (!target) {
                return std::unexpected(target.error());
            }

            auto value =
                analyze_value_expression(*assignment->value, target->type, "assignment value");
            if (!value) {
                return std::unexpected(value.error());
            }

            if (!same_type(value->type, target->type)) {
                return std::unexpected(make_error(
                    assignment->value->range,
                    "cannot assign value of type '" + value->type->name +
                        "' to target of type '" + target->type->name + '\''));
            }

            return make_value_info(expression, target->type);
        }

        if (const auto* call = dynamic_cast<const AST::CallExpr*>(&expression)) {
            auto callee = analyze_expression(*call->callee);
            if (!callee) {
                return std::unexpected(callee.error());
            }

            if (callee->function == nullptr) {
                return std::unexpected(
                    make_error(call->callee->range, "expression is not callable"));
            }

            return analyze_call(*call, *callee->function);
        }

        if (dynamic_cast<const AST::CastExpr*>(&expression) != nullptr) {
            return std::unexpected(
                make_error(expression.range, "TODO: cast expressions are not implemented in this stage"));
        }

        if (dynamic_cast<const AST::FieldAccessExpr*>(&expression) != nullptr) {
            return std::unexpected(
                make_error(expression.range, "TODO: field access is not implemented in this stage"));
        }

        if (dynamic_cast<const AST::IndexExpr*>(&expression) != nullptr) {
            return std::unexpected(
                make_error(expression.range, "TODO: array indexing is not implemented in this stage"));
        }

        if (dynamic_cast<const AST::ArrayLiteralExpr*>(&expression) != nullptr) {
            return std::unexpected(
                make_error(expression.range, "TODO: array literals are not implemented in this stage"));
        }

        if (dynamic_cast<const AST::StructLiteralExpr*>(&expression) != nullptr) {
            return std::unexpected(
                make_error(expression.range, "TODO: struct literals are not implemented in this stage"));
        }

        return std::unexpected(make_error(expression.range, "unsupported expression kind"));
    }

    std::expected<ExprInfo, SemanticError> analyze_assignment_target(const AST::Expr& expression) {
        // поддерживаем пока только самое простое присваивание
        if (dynamic_cast<const AST::IdentifierExpr*>(&expression) == nullptr) {
            return std::unexpected(make_error(
                expression.range,
                "TODO: only simple identifier assignment is implemented in this stage"));
        }

        auto target = analyze_expression(expression);
        if (!target) {
            return std::unexpected(target.error());
        }

        if (!target->is_lvalue) {
            return std::unexpected(make_error(expression.range, "invalid assignment target"));
        }

        if (!target->is_mutable_lvalue) {
            return std::unexpected(
                make_error(expression.range, "cannot assign to an immutable value"));
        }

        return target;
    }

    std::expected<ExprInfo, SemanticError> analyze_binary_expression(const AST::BinaryExpr& binary) {
        ExprInfo left;
        ExprInfo right;

        //если одна сторона числовой литерал, удобно сначала разобрать
        // вторую сторону и попытаться подтянуть literal к её типу
        if (is_numeric_literal_expr(*binary.left) && !is_numeric_literal_expr(*binary.right)) {
            auto analyzed_right = analyze_value_expression(*binary.right, nullptr, "right operand");
            if (!analyzed_right) {
                return std::unexpected(analyzed_right.error());
            }
            right = *analyzed_right;

            auto analyzed_left = analyze_value_expression(*binary.left, right.type, "left operand");
            if (!analyzed_left) {
                return std::unexpected(analyzed_left.error());
            }
            left = *analyzed_left;
        } else {
            auto analyzed_left = analyze_value_expression(*binary.left, nullptr, "left operand");
            if (!analyzed_left) {
                return std::unexpected(analyzed_left.error());
            }
            left = *analyzed_left;

            auto analyzed_right = analyze_value_expression(*binary.right, left.type, "right operand");
            if (!analyzed_right) {
                return std::unexpected(analyzed_right.error());
            }
            right = *analyzed_right;
        }

        switch (binary.op_type) {
            case Lexer::TokenType::Plus:
                if (left.type->is_string() && right.type->is_string()) {
                    return make_value_info(binary, builtin_type("string"));
                }
                [[fallthrough]];
            case Lexer::TokenType::Minus:
            case Lexer::TokenType::Star:
            case Lexer::TokenType::Slash:
            case Lexer::TokenType::Percent:
                if (!same_type(left.type, right.type) || !left.type->is_numeric()) {
                    return std::unexpected(make_error(
                        binary.range,
                        "operator '" + binary.op_lexeme +
                            "' requires operands of the same numeric type"));
                }
                return make_value_info(binary, left.type);

            case Lexer::TokenType::AmpAmp:
            case Lexer::TokenType::PipePipe:
                if (!same_type(left.type, builtin_type("bool")) ||
                    !same_type(right.type, builtin_type("bool"))) {
                    return std::unexpected(make_error(
                        binary.range, "logical operators require operands of type 'bool'"));
                }
                return make_value_info(binary, builtin_type("bool"));

            case Lexer::TokenType::EqualEqual:
            case Lexer::TokenType::BangEqual:
                if (!same_type(left.type, right.type) ||
                    !(left.type->is_numeric() || left.type->is_bool() || left.type->is_string())) {
                    return std::unexpected(make_error(
                        binary.range,
                        "operator '" + binary.op_lexeme +
                            "' requires comparable operands of the same type"));
                }
                return make_value_info(binary, builtin_type("bool"));

            case Lexer::TokenType::Less:
            case Lexer::TokenType::Greater:
            case Lexer::TokenType::LessEqual:
            case Lexer::TokenType::GreaterEqual:
                if (!same_type(left.type, right.type) || !left.type->is_numeric()) {
                    return std::unexpected(make_error(
                        binary.range,
                        "relational operators require operands of the same numeric type"));
                }
                return make_value_info(binary, builtin_type("bool"));

            default:
                return std::unexpected(make_error(binary.range, "unsupported binary operator"));
        }
    }

    std::expected<ExprInfo, SemanticError> analyze_call(const AST::CallExpr& call,
        switch (function.builtin_kind) {
            case BuiltinKind::Print: {
                if (call.arguments.size() != 1) {
                    return std::unexpected(
                        make_error(call.range, "'print' expects exactly 1 argument"));
                }

                auto argument =
                    analyze_value_expression(*call.arguments[0], nullptr, "builtin argument");
                if (!argument) {
                    return std::unexpected(argument.error());
                }

                if (!is_printable_type(argument->type)) {
                    return std::unexpected(make_error(
                        call.arguments[0]->range,
                        "'print' only accepts integer, floating-point, bool, or string values"));
                }

                return make_value_info(call, builtin_type("void"));
            }

            case BuiltinKind::Input:
                if (!call.arguments.empty()) {
                    return std::unexpected(
                        make_error(call.range, "'input' expects no arguments"));
                }
                return make_value_info(call, builtin_type("string"));

            case BuiltinKind::Len: {
                if (call.arguments.size() != 1) {
                    return std::unexpected(
                        make_error(call.range, "'len' expects exactly 1 argument"));
                }

                auto argument = analyze_value_expression(*call.arguments[0], builtin_type("string"),
                                                         "builtin argument");
                if (!argument) {
                    return std::unexpected(argument.error());
                }

                if (!same_type(argument->type, builtin_type("string"))) {
                    return std::unexpected(
                        make_error(call.arguments[0]->range, "'len' expects an argument of type 'string'"));
                }

                return make_value_info(call, builtin_type("int32"));
            }

            case BuiltinKind::Exit: {
                if (call.arguments.size() != 1) {
                    return std::unexpected(
                        make_error(call.range, "'exit' expects exactly 1 argument"));
                }

                auto argument = analyze_value_expression(*call.arguments[0], builtin_type("int32"),
                                                         "builtin argument");
                if (!argument) {
                    return std::unexpected(argument.error());
                }

                if (!same_type(argument->type, builtin_type("int32"))) {
                    return std::unexpected(
                        make_error(call.arguments[0]->range, "'exit' expects an argument of type 'int32'"));
                }

                return make_value_info(call, builtin_type("void"));
            }

            case BuiltinKind::Panic: {
                if (call.arguments.size() != 1) {
                    return std::unexpected(
                        make_error(call.range, "'panic' expects exactly 1 argument"));
                }

                auto argument = analyze_value_expression(*call.arguments[0], builtin_type("string"),
                                                         "builtin argument");
                if (!argument) {
                    return std::unexpected(argument.error());
                }

                if (!same_type(argument->type, builtin_type("string"))) {
                    return std::unexpected(make_error(
                        call.arguments[0]->range, "'panic' expects an argument of type 'string'"));
                }

                return make_value_info(call, builtin_type("void"));
            }

            case BuiltinKind::None:
                break;
        }

        if (call.arguments.size() != function.parameter_types.size()) {
            return std::unexpected(make_error(
                call.range,
                "function '" + function.full_name + "' expects " +
                    std::to_string(function.parameter_types.size()) + " argument(s), got " +
                    std::to_string(call.arguments.size())));
        }

        for (std::size_t i = 0; i < call.arguments.size(); ++i) {
            auto argument = analyze_value_expression(*call.arguments[i], function.parameter_types[i],
                                                     "function argument");
            if (!argument) {
                return std::unexpected(argument.error());
            }

            if (!same_type(argument->type, function.parameter_types[i])) {
                return std::unexpected(make_error(
                    call.arguments[i]->range,
                    "argument " + std::to_string(i + 1) + " of function '" + function.full_name +
                        "' has type '" + argument->type->name + "', expected '" +
                        function.parameter_types[i]->name + '\''));
            }
        }

        return make_value_info(call, function.return_type);
    }
};

std::expected<SemanticResult, SemanticError> analyze_program(const AST::Program& program,
                                                             std::string filename) {
    Analyzer analyzer(std::move(filename));
    return analyzer.analyze(program);
}

}