#include "codogen.hpp"

#include <sstream>
#include <string>
#include <unordered_map>

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
  public:
    Generator(const AST::Program& program, const Semantic::SemanticResult& semantic_result,
              std::string filename)
        : program_(program),
          semantic_result_(semantic_result),
          filename_(std::move(filename)) {}

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

    [[nodiscard]] CodegenError make_error(const Lexer::SourceRange& range,
                                          std::string message) const {
        return CodegenError {
            .filename = filename_,
            .message = std::move(message),
            .location = range.begin,
        };
    }

    std::expected<void, CodegenError> emit_function(const AST::FunctionDecl& function) {
        const auto it = semantic_result_.functions.find(&function);
        if (it == semantic_result_.functions.end()) {
            return std::unexpected(
                make_error(function.range, "missing semantic info for function"));
        }

        const std::string label = mangle_name(it->second.full_name);
        text_ << ".globl " << label << '\n';
        text_ << label << ":\n";
        text_ << "    push rbp\n";
        text_ << "    mov rbp, rsp\n";

        for (const auto& statement : function.body->statements) {
            const auto* return_stmt = dynamic_cast<const AST::ReturnStmt*>(statement.get());
            if (return_stmt == nullptr || return_stmt->value == nullptr) {
                return std::unexpected(make_error(
                    statement->range,
                    "stage 1 codegen supports only 'return <literal>;' inside functions"));
            }

            auto emitted = emit_expression(*return_stmt->value);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }
            text_ << "    leave\n";
            text_ << "    ret\n\n";
            return {};
        }

        text_ << "    mov rax, 0\n";
        text_ << "    leave\n";
        text_ << "    ret\n\n";
        return {};
    }

    std::expected<void, CodegenError> emit_expression(const AST::Expr& expression) {
        if (const auto* literal = dynamic_cast<const AST::IntLiteralExpr*>(&expression)) {
            text_ << "    mov rax, " << literal->value << '\n';
            return {};
        }

        if (const auto* literal = dynamic_cast<const AST::BoolLiteralExpr*>(&expression)) {
            text_ << "    mov rax, " << (literal->value ? 1 : 0) << '\n';
            return {};
        }

        return std::unexpected(make_error(
            expression.range, "stage 1 supports only integer and bool literal expressions"));
    }
};

std::expected<std::string, CodegenError> generate_program(
    const AST::Program& program, const Semantic::SemanticResult& semantic_result,
    std::string filename) {
    Generator generator(program, semantic_result, std::move(filename));
    return generator.generate();
}

}