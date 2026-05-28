#include "codogen.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string mangle_name(std::string_view full_name) {
    if (full_name == "main") {
        return "main";
    }
    return "__minic_fn_" + std::string(full_name);
}

}  // namespace

namespace Codegen {

std::string format_error(const CodegenError& error) {
    std::ostringstream stream;
    stream << error.filename << ':' << error.location.line << ':' << error.location.column
           << ": error: " << error.message;
    return stream.str();
}

class Generator {
  private:
    struct Storage {
        int offset = 0;
    };

    struct FunctionContext {
        std::ostringstream body;
        std::vector<std::unordered_map<std::string, Storage>> scopes;
        std::vector<std::string> break_labels;
        std::vector<std::string> continue_labels;
        int next_stack_offset = 0;
        std::string exit_label {};
    };

  public:
    Generator(const AST::Program& program, const Semantic::SemanticResult& semantic_result,
              std::string filename)
        : program_(program),
          semantic_result_(semantic_result),
          filename_(std::move(filename)) {
        for (const auto& [decl, info] : semantic_result_.functions) {
            function_names_.emplace(decl, info.full_name);
        }
    }

    std::expected<std::string, CodegenError> generate() {
        text_ << ".intel_syntax noprefix\n";
        text_ << ".text\n\n";

        for (const auto& declaration : program_.declarations) {
            const auto* function = dynamic_cast<const AST::FunctionDecl*>(declaration.get());
            if (function == nullptr) {
                continue;
            }

            auto emitted = emit_function(*function);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }
        }

        return text_.str();
    }

  private:
    const AST::Program& program_;
    const Semantic::SemanticResult& semantic_result_;
    std::string filename_;
    std::ostringstream text_;
    int label_counter_ = 0;
    std::unordered_map<const AST::FunctionDecl*, std::string> function_names_;
    FunctionContext* current_function_ = nullptr;

    [[nodiscard]] CodegenError make_error(const Lexer::SourceRange& range,
                                          std::string message) const {
        return CodegenError {
            .filename = filename_,
            .message = std::move(message),
            .location = range.begin,
        };
    }

    [[nodiscard]] std::string new_label(std::string_view prefix) {
        return std::string(prefix) + std::to_string(label_counter_++);
    }

    void emit_body_line(const std::string& text) { current_function_->body << "    " << text << '\n'; }
    void emit_body_label(const std::string& label) { current_function_->body << label << ":\n"; }

    int allocate_stack_slot() {
        current_function_->next_stack_offset += 8;
        return current_function_->next_stack_offset;
    }

    std::expected<void, CodegenError> emit_function(const AST::FunctionDecl& function) {
        FunctionContext context;
        context.scopes.push_back({});
        context.exit_label = new_label(".Lreturn");
        current_function_ = &context;

        auto emitted = emit_block(*function.body, false);
        if (!emitted) {
            current_function_ = nullptr;
            return std::unexpected(emitted.error());
        }

        emit_body_label(context.exit_label);
        emit_body_line("leave");
        emit_body_line("ret");

        int stack_size = context.next_stack_offset;
        if (stack_size % 16 != 0) {
            stack_size += 16 - (stack_size % 16);
        }

        const auto label = mangle_name(function_names_.at(&function));
        text_ << ".globl " << label << '\n';
        text_ << label << ":\n";
        text_ << "    push rbp\n";
        text_ << "    mov rbp, rsp\n";
        if (stack_size > 0) {
            text_ << "    sub rsp, " << stack_size << '\n';
        }
        text_ << context.body.str() << '\n';

        current_function_ = nullptr;
        return {};
    }

    std::expected<void, CodegenError> emit_block(const AST::BlockStmt& block, bool create_scope) {
        if (create_scope) {
            current_function_->scopes.push_back({});
        }

        for (const auto& statement : block.statements) {
            auto emitted = emit_statement(*statement);
            if (!emitted) {
                if (create_scope) {
                    current_function_->scopes.pop_back();
                }
                return std::unexpected(emitted.error());
            }
        }

        if (create_scope) {
            current_function_->scopes.pop_back();
        }
        return {};
    }

    std::expected<void, CodegenError> emit_statement(const AST::Stmt& statement) {
        if (const auto* block = dynamic_cast<const AST::BlockStmt*>(&statement)) {
            return emit_block(*block, true);
        }

        if (const auto* declaration = dynamic_cast<const AST::VariableDeclStmt*>(&statement)) {
            const int offset = allocate_stack_slot();
            auto emitted = emit_expression(*declaration->initializer);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(offset) + "], rax");
            current_function_->scopes.back().emplace(declaration->name, Storage {.offset = offset});
            return {};
        }

        if (const auto* if_stmt = dynamic_cast<const AST::IfStmt*>(&statement)) {
            const auto else_label = new_label(".Lelse");
            const auto end_label = new_label(".Lendif");
            auto condition = emit_expression(*if_stmt->condition);
            if (!condition) {
                return std::unexpected(condition.error());
            }
            emit_body_line("cmp rax, 0");
            emit_body_line("je " + (if_stmt->else_branch ? else_label : end_label));
            auto then_emitted = emit_block(*if_stmt->then_branch, true);
            if (!then_emitted) {
                return std::unexpected(then_emitted.error());
            }
            if (if_stmt->else_branch) {
                emit_body_line("jmp " + end_label);
                emit_body_label(else_label);
                auto else_emitted = emit_block(*if_stmt->else_branch, true);
                if (!else_emitted) {
                    return std::unexpected(else_emitted.error());
                }
            }
            emit_body_label(end_label);
            return {};
        }

        if (const auto* while_stmt = dynamic_cast<const AST::WhileStmt*>(&statement)) {
            const auto start_label = new_label(".Lwhile_start");
            const auto body_label = new_label(".Lwhile_body");
            const auto end_label = new_label(".Lwhile_end");

            current_function_->continue_labels.push_back(start_label);
            current_function_->break_labels.push_back(end_label);

            emit_body_label(start_label);
            auto condition = emit_expression(*while_stmt->condition);
            if (!condition) {
                return std::unexpected(condition.error());
            }
            emit_body_line("cmp rax, 0");
            emit_body_line("jne " + body_label);
            emit_body_line("jmp " + end_label);
            emit_body_label(body_label);
            auto body_emitted = emit_block(*while_stmt->body, true);
            current_function_->continue_labels.pop_back();
            current_function_->break_labels.pop_back();
            if (!body_emitted) {
                return std::unexpected(body_emitted.error());
            }
            emit_body_line("jmp " + start_label);
            emit_body_label(end_label);
            return {};
        }

        if (const auto* return_stmt = dynamic_cast<const AST::ReturnStmt*>(&statement)) {
            if (return_stmt->value != nullptr) {
                auto emitted = emit_expression(*return_stmt->value);
                if (!emitted) {
                    return std::unexpected(emitted.error());
                }
            }
            emit_body_line("jmp " + current_function_->exit_label);
            return {};
        }

        if (dynamic_cast<const AST::BreakStmt*>(&statement) != nullptr) {
            emit_body_line("jmp " + current_function_->break_labels.back());
            return {};
        }

        if (dynamic_cast<const AST::ContinueStmt*>(&statement) != nullptr) {
            emit_body_line("jmp " + current_function_->continue_labels.back());
            return {};
        }

        if (const auto* expression_stmt = dynamic_cast<const AST::ExprStmt*>(&statement)) {
            return emit_expression(*expression_stmt->expression);
        }

        if (dynamic_cast<const AST::EmptyStmt*>(&statement) != nullptr) {
            return {};
        }

        return std::unexpected(make_error(statement.range, "unsupported statement at stage 2"));
    }

    std::expected<void, CodegenError> emit_expression(const AST::Expr& expression) {
        if (const auto* identifier = dynamic_cast<const AST::IdentifierExpr*>(&expression)) {
            auto storage = lookup_storage(identifier->name, expression.range);
            if (!storage) {
                return std::unexpected(storage.error());
            }
            emit_body_line("mov rax, qword ptr [rbp - " + std::to_string(storage->offset) + "]");
            return {};
        }

        if (const auto* literal = dynamic_cast<const AST::IntLiteralExpr*>(&expression)) {
            emit_body_line("mov rax, " + literal->value);
            return {};
        }

        if (const auto* literal = dynamic_cast<const AST::BoolLiteralExpr*>(&expression)) {
            emit_body_line(std::string("mov rax, ") + (literal->value ? "1" : "0"));
            return {};
        }

        if (const auto* unary = dynamic_cast<const AST::UnaryExpr*>(&expression)) {
            auto emitted = emit_expression(*unary->operand);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }
            if (unary->op_type == Lexer::TokenType::Minus) {
                emit_body_line("neg rax");
                return {};
            }
            if (unary->op_type == Lexer::TokenType::Bang) {
                emit_body_line("cmp rax, 0");
                emit_body_line("sete al");
                emit_body_line("movzx rax, al");
                return {};
            }
            return std::unexpected(make_error(expression.range, "unsupported unary op at stage 2"));
        }

        if (const auto* binary = dynamic_cast<const AST::BinaryExpr*>(&expression)) {
            const int temp_offset = allocate_stack_slot();
            auto left = emit_expression(*binary->left);
            if (!left) {
                return std::unexpected(left.error());
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(temp_offset) + "], rax");
            auto right = emit_expression(*binary->right);
            if (!right) {
                return std::unexpected(right.error());
            }
            emit_body_line("mov rcx, rax");
            emit_body_line("mov rax, qword ptr [rbp - " + std::to_string(temp_offset) + "]");

            switch (binary->op_type) {
                case Lexer::TokenType::Plus:
                    emit_body_line("add rax, rcx");
                    return {};
                case Lexer::TokenType::Minus:
                    emit_body_line("sub rax, rcx");
                    return {};
                case Lexer::TokenType::Star:
                    emit_body_line("imul rax, rcx");
                    return {};
                case Lexer::TokenType::EqualEqual:
                    emit_body_line("cmp rax, rcx");
                    emit_body_line("sete al");
                    emit_body_line("movzx rax, al");
                    return {};
                case Lexer::TokenType::Less:
                    emit_body_line("cmp rax, rcx");
                    emit_body_line("setl al");
                    emit_body_line("movzx rax, al");
                    return {};
                default:
                    return std::unexpected(make_error(
                        expression.range, "unsupported binary op at stage 2"));
            }
        }

        return std::unexpected(make_error(expression.range, "unsupported expression at stage 2"));
    }

    std::expected<Storage, CodegenError> lookup_storage(const std::string& name,
                                                        const Lexer::SourceRange& range) const {
        for (auto it = current_function_->scopes.rbegin(); it != current_function_->scopes.rend();
             ++it) {
            const auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return std::unexpected(make_error(range, "unknown local '" + name + '\''));
    }
};

std::expected<std::string, CodegenError> generate_program(
    const AST::Program& program, const Semantic::SemanticResult& semantic_result,
    std::string filename) {
    Generator generator(program, semantic_result, std::move(filename));
    return generator.generate();
}

}