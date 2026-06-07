#include "codegen.hpp"
#include <algorithm>
#include <bit>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

//проверяет, является ли имя одним из целочисленных типов языка
bool is_integer_name(std::string_view name) {
    return name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
           name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64";
}

//проверяет, является ли тип беззнаковым целым
bool is_unsigned_integer_name(std::string_view name) {
    return name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64";
}

//проверка на float32 / float64
bool is_float_name(std::string_view name) {
    return name == "float32" || name == "float64";
}

//отдельная проверка на char
bool is_char_name(std::string_view name) {
    return name == "char";
}

//округляет размер вверх до нужного выравнивания
//используется в stack frame и при расчёте размеров структур
int align_to(int value, int alignment) {
    const int remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }
    return value + (alignment - remainder);
}

//берёт текст строкового литерала с кавычками и escape-последовательностями
//и превращает его в реальную строку, которую потом можно положить в .rodata
std::string decode_string_literal(std::string_view lexeme) {
    std::string value;
    if (lexeme.size() < 2) {
        return value;
    }

    for (std::size_t i = 1; i + 1 < lexeme.size(); ++i) {
        if (lexeme[i] != '\\') {
            value.push_back(lexeme[i]);
            continue;
        }

        ++i;
        switch (lexeme[i]) {
            case '\\':
                value.push_back('\\');
                break;
            case '"':
                value.push_back('"');
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                value.push_back(lexeme[i]);
                break;
        }
    }

    return value;
}

//разбирает целочисленный литерал в число
//поддерживает десятичную, hex (0x) и binary (0b) формы
std::expected<std::uint64_t, std::string> parse_unsigned_integer_literal(std::string_view text) {
    int base = 10;
    std::size_t start = 0;

    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        start = 2;
    } else if (text.size() > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2;
        start = 2;
    }

    if (start == text.size()) {
        return std::unexpected("missing digits");
    }

    std::uint64_t value = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        const char ch = text[i];
        int digit = -1;
        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            digit = 10 + (ch - 'a');
        } else if (ch >= 'A' && ch <= 'F') {
            digit = 10 + (ch - 'A');
        }

        if (digit < 0 || digit >= base) {
            return std::unexpected("invalid digit");
        }

        value = value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
    }

    return value;
}

//разбирает символьный литерал вроде 'A' или '\n'
//и возвращает числовой код символа
std::expected<std::uint64_t, std::string> decode_char_literal(std::string_view lexeme) {
    if (lexeme.size() < 3 || lexeme.front() != '\'' || lexeme.back() != '\'') {
        return std::unexpected("malformed character literal");
    }

    if (lexeme[1] != '\\') {
        if (lexeme.size() != 3) {
            return std::unexpected("character literal must contain exactly one character");
        }
        return static_cast<unsigned char>(lexeme[1]);
    }

    if (lexeme.size() != 4) {
        return std::unexpected("character literal must contain exactly one escaped character");
    }

    switch (lexeme[2]) {
        case '\\':
            return static_cast<std::uint64_t>('\\');
        case '\'':
            return static_cast<std::uint64_t>('\'');
        case '"':
            return static_cast<std::uint64_t>('"');
        case 'n':
            return static_cast<std::uint64_t>('\n');
        case 't':
            return static_cast<std::uint64_t>('\t');
        default:
            return std::unexpected("unsupported character escape");
    }
}

//экранирует строку так, чтобы её можно было безопасно записать в asm как .asciz
std::string escape_asm_string(std::string_view value) {
    std::string escaped;
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 32 ||
                    static_cast<unsigned char>(ch) >= 127) {
                    char buffer[5];
                    std::snprintf(buffer, sizeof(buffer), "\\%03o",
                                  static_cast<unsigned char>(ch));
                    escaped += buffer;
                } else {
                    escaped.push_back(ch);
                }
                break;
        }
    }
    return escaped;
}

//превращает полное имя функции в безопасный label для assembly
//например Math::sum -> __minic_fn_Math__sum
std::string mangle_name(std::string_view full_name) {
    if (full_name == "main") {
        return "main";
    }

    std::string result = "__minic_fn_";
    for (const char ch : full_name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
            result.push_back(ch);
        } else {
            result.push_back('_');
        }
    }
    return result;
}

//форматирует 32-битное значение как hex-строку для asm
std::string format_u32_hex(std::uint32_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

//форматирует 64-битное значение как hex-строку для asm
std::string format_u64_hex(std::uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

}  // namespace

namespace Codegen {

//собирает диагностику codegen в общий строковый формат
std::string format_error(const CodegenError& error) {
    std::ostringstream stream;
    stream << error.filename << ':' << error.location.line << ':' << error.location.column
           << ": error: " << error.message;
    return stream.str();
}

//главный backend-объект
//он получает AST + SemanticResult и постепенно собирает текст x86-64 assembly
class Generator {
  private:
    //регистры, через которые по ABI передаются первые аргументы функций
    static constexpr const char* kArgumentRegisters[6] = {"rdi", "rsi", "rdx",
                                                           "rcx", "r8",  "r9"};

    //описание локального storage: тип переменной и её смещение в stack frame
    struct Storage {
        Semantic::SemanticType type {};
        int offset = 0;
    };

    //вся временная информация о функции во время генерации
    struct FunctionContext {
        std::ostringstream body;
        std::vector<std::unordered_map<std::string, Storage>> scopes;
        std::vector<std::string> break_labels;
        std::vector<std::string> continue_labels;
        std::vector<int> temp_slots;
        int temp_depth = 0;
        int next_stack_offset = 0;
        std::string exit_label {};
        Semantic::SemanticType return_type {};
        std::optional<int> aggregate_return_pointer_offset;
    };

  public:
    //конструктор запоминает программу и семантические таблицы
    //и сразу подготавливает быстрый доступ к функциям и структурам
    Generator(const AST::Program& program, const Semantic::SemanticResult& semantic_result,
              std::string filename)
        : program_(program),
          semantic_result_(semantic_result),
          filename_(std::move(filename)) {
        for (const auto& [decl, info] : semantic_result_.functions) {
            functions_by_name_.emplace(info.full_name, &info);
            function_names_.emplace(decl, info.full_name);
        }

        for (const auto& [_, info] : semantic_result_.structs) {
            structs_by_name_.emplace(info.full_name, &info);
        }
    }

    //главная функция backend
    //собирает секции .text и .rodata, а потом возвращает весь asm как одну строку
    std::expected<std::string, CodegenError> generate() {
        text_ << ".intel_syntax noprefix\n";
        text_ << ".text\n";
        text_ << ".extern strlen\n";
        text_ << ".extern strcmp\n";
        text_ << ".extern memcpy\n";
        text_ << ".extern exit\n\n";

        auto emitted = emit_declarations(program_.declarations);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }

        std::ostringstream output;
        output << text_.str();
        output << "\n.section .rodata\n";
        output << rodata_.str();
        output << "\n.section .note.GNU-stack,\"\",@progbits\n";
        return output.str();
    }

  private:
    const AST::Program& program_;
    const Semantic::SemanticResult& semantic_result_;
    std::string filename_;
    std::ostringstream text_;   //сюда пишем исполняемый код функций
    std::ostringstream rodata_; //сюда пишем строковые литералы и другие readonly данные
    int label_counter_ = 0;
    int string_counter_ = 0;
    std::unordered_map<std::string, std::string> string_labels_;
    std::unordered_map<const AST::FunctionDecl*, std::string> function_names_;
    std::unordered_map<std::string, const Semantic::FunctionInfo*> functions_by_name_;
    std::unordered_map<std::string, const Semantic::StructInfo*> structs_by_name_;
    FunctionContext* current_function_ = nullptr;

    [[nodiscard]] CodegenError make_error(const Lexer::SourceRange& range,
                                          std::string message) const {
        return CodegenError {
            .filename = filename_,
            .message = std::move(message),
            .location = range.begin,
        };
    }

    [[nodiscard]] CodegenError make_error(const Lexer::SourceLocation& location,
                                          std::string message) const {
        return CodegenError {
            .filename = filename_,
            .message = std::move(message),
            .location = location,
        };
    }

    //проходит по top-level объявлениям и генерирует код только для функций
    //struct/alias/namespace сами по себе asm не дают
    std::expected<void, CodegenError> emit_declarations(
        const std::vector<std::unique_ptr<AST::Decl>>& declarations) {
        for (const auto& declaration : declarations) {
            if (const auto* name_space =
                    dynamic_cast<const AST::NamespaceDecl*>(declaration.get())) {
                auto nested = emit_declarations(name_space->declarations);
                if (!nested) {
                    return std::unexpected(nested.error());
                }
                continue;
            }

            const auto* function = dynamic_cast<const AST::FunctionDecl*>(declaration.get());
            if (function == nullptr) {
                continue;
            }

            auto emitted = emit_function(*function);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }
        }

        return {};
    }

    //генерация одной функции целиком:
    //пролог, параметры, тело, общий выход и эпилог
    std::expected<void, CodegenError> emit_function(const AST::FunctionDecl& function) {
        const auto info_it = semantic_result_.functions.find(&function);
        if (info_it == semantic_result_.functions.end()) {
            return std::unexpected(
                make_error(function.range, "internal error: missing semantic function info"));
        }

        const auto& function_info = info_it->second;
        //если функция возвращает struct/array,
        //результат передаётся через скрытый указатель на память вызывающей стороны
        const bool aggregate_return = is_aggregate_runtime_type(function_info.return_type);
        const std::size_t required_registers =
            function_info.parameter_types.size() + (aggregate_return ? 1 : 0);
        if (required_registers > 6) {
            return std::unexpected(make_error(
                function.range,
                "codegen currently supports at most 6 register arguments per function"));
        }

        if (function_info.return_type.name != "void") {
            auto supported = ensure_supported_type(function_info.return_type, function.range);
            if (!supported) {
                return std::unexpected(supported.error());
            }
        }

        FunctionContext context;
        context.exit_label = new_label(".Lreturn");
        context.return_type = function_info.return_type;
        context.scopes.push_back({});
        current_function_ = &context;

        std::size_t register_index = 0;
        if (aggregate_return) {
            //сохраняем hidden return pointer в локальный слот функции
            const int hidden_offset = allocate_stack_slot();
            context.aggregate_return_pointer_offset = hidden_offset;
            emit_body_line("mov qword ptr [rbp - " + std::to_string(hidden_offset) + "], " +
                           std::string(kArgumentRegisters[register_index]));
            ++register_index;
        }

        std::vector<int> incoming_parameter_slots;
        incoming_parameter_slots.reserve(function.parameters.size());
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            const auto& parameter = function.parameters[i];
            const auto& parameter_type = function_info.parameter_types[i];

            auto supported = ensure_supported_type(parameter_type, parameter.range);
            if (!supported) {
                current_function_ = nullptr;
                return std::unexpected(supported.error());
            }

            const int incoming_offset = allocate_stack_slot();
            //сначала сохраняем все входные аргументы из регистров в стек
            emit_body_line("mov qword ptr [rbp - " + std::to_string(incoming_offset) + "], " +
                           std::string(kArgumentRegisters[register_index]));
            incoming_parameter_slots.push_back(incoming_offset);
            ++register_index;
        }

        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            const auto& parameter = function.parameters[i];
            const auto& parameter_type = function_info.parameter_types[i];
            const int incoming_offset = incoming_parameter_slots[i];

            if (is_scalar_runtime_type(parameter_type)) {
                //скаляры можно просто хранить как 64-битное значение в стеке
                current_function_->scopes.back().emplace(parameter.name, Storage {
                                                                         .type = parameter_type,
                                                                         .offset = incoming_offset,
                                                                     });
                continue;
            }

            auto size = type_size(parameter_type, parameter.range);
            if (!size) {
                current_function_ = nullptr;
                return std::unexpected(size.error());
            }

            const int local_offset = allocate_stack_bytes(*size);
            current_function_->scopes.back().emplace(parameter.name, Storage {
                                                                     .type = parameter_type,
                                                                     .offset = local_offset,
                                                                 });
            //aggregate-параметры копируем по значению через memcpy
            emit_body_line("lea rdi, [rbp - " + std::to_string(local_offset) + "]");
            emit_body_line("mov rsi, qword ptr [rbp - " + std::to_string(incoming_offset) + "]");
            emit_body_line("mov rdx, " + std::to_string(*size));
            emit_body_line("call memcpy");
        }

        auto emitted = emit_block(*function.body, false);
        if (!emitted) {
            current_function_ = nullptr;
            return std::unexpected(emitted.error());
        }

        emit_body_label(context.exit_label);
        if (context.aggregate_return_pointer_offset.has_value()) {
            //для aggregate-return в rax возвращаем адрес буфера результата
            emit_body_line("mov rax, qword ptr [rbp - " +
                           std::to_string(*context.aggregate_return_pointer_offset) + "]");
        }
        emit_body_line("leave");
        emit_body_line("ret");

        int stack_size = context.next_stack_offset;
        if (stack_size % 16 != 0) {
            stack_size += 16 - (stack_size % 16);
        }

        const auto label = mangle_name(function_info.full_name);
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

    //генерация блока { ... }
    //при необходимости открывает отдельный scope локальных переменных
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

    //генерация одной инструкции
    std::expected<void, CodegenError> emit_statement(const AST::Stmt& statement) {
        if (const auto* block = dynamic_cast<const AST::BlockStmt*>(&statement)) {
            return emit_block(*block, true);
        }

        //локальная переменная
        if (const auto* declaration = dynamic_cast<const AST::VariableDeclStmt*>(&statement)) {
            auto type = lookup_variable_type(*declaration);
            if (!type) {
                return std::unexpected(type.error());
            }

            auto supported = ensure_supported_type(*type, declaration->range);
            if (!supported) {
                return std::unexpected(supported.error());
            }

            const int storage_size = *type_size(*type, declaration->range);
            const int offset = allocate_stack_bytes(storage_size);

            if (is_scalar_runtime_type(*type)) {
                //для scalar-значений вычисляем initializer в rax и кладём в стек
                auto initializer = emit_expression(*declaration->initializer);
                if (!initializer) {
                    return std::unexpected(initializer.error());
                }
                auto normalized = normalize_rax(*type, declaration->range);
                if (!normalized) {
                    return std::unexpected(normalized.error());
                }
                emit_body_line("mov qword ptr [rbp - " + std::to_string(offset) + "], rax");
            } else {
                //для array/struct сначала получаем адрес места назначения,
                //а потом копируем туда всё значение
                emit_body_line("lea rdi, [rbp - " + std::to_string(offset) + "]");
                auto stored =
                    emit_store_expression_to_rdi(*declaration->initializer, *type);
                if (!stored) {
                    return std::unexpected(stored.error());
                }
            }

            current_function_->scopes.back().emplace(declaration->name, Storage {
                                                                          .type = *type,
                                                                          .offset = offset,
                                                                      });
            return {};
        }

        //if / else через label и условные прыжки
        if (const auto* if_stmt = dynamic_cast<const AST::IfStmt*>(&statement)) {
            const auto else_label = new_label(".Lelse");
            const auto end_label = new_label(".Lendif");

            auto condition = emit_expression(*if_stmt->condition);
            if (!condition) {
                return std::unexpected(condition.error());
            }

            emit_body_line("cmp rax, 0");
            if (if_stmt->else_branch != nullptr) {
                emit_body_line("je " + else_label);
            } else {
                emit_body_line("je " + end_label);
            }

            auto then_emitted = emit_block(*if_stmt->then_branch, true);
            if (!then_emitted) {
                return std::unexpected(then_emitted.error());
            }

            if (if_stmt->else_branch != nullptr) {
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

        //while тоже lowering в label + jmp
        if (const auto* while_stmt = dynamic_cast<const AST::WhileStmt*>(&statement)) {
            const auto start_label = new_label(".Lwhile_start");
            const auto body_label = new_label(".Lwhile_body");
            const auto end_label = new_label(".Lwhile_end");

            current_function_->continue_labels.push_back(start_label);
            current_function_->break_labels.push_back(end_label);

            emit_body_label(start_label);
            auto condition = emit_expression(*while_stmt->condition);
            if (!condition) {
                current_function_->continue_labels.pop_back();
                current_function_->break_labels.pop_back();
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

        //return
        if (const auto* return_stmt = dynamic_cast<const AST::ReturnStmt*>(&statement)) {
            if (return_stmt->value != nullptr) {
                if (is_aggregate_runtime_type(current_function_->return_type)) {
                    //если возвращаем array/struct,
                    //пишем результат по скрытому адресу, который дал caller
                    if (!current_function_->aggregate_return_pointer_offset.has_value()) {
                        return std::unexpected(make_error(
                            statement.range,
                            "internal error: missing aggregate return pointer"));
                    }

                    emit_body_line("mov rdi, qword ptr [rbp - " +
                                   std::to_string(*current_function_->aggregate_return_pointer_offset) +
                                   "]");
                    auto stored = emit_store_expression_to_rdi(*return_stmt->value,
                                                               current_function_->return_type);
                    if (!stored) {
                        return std::unexpected(stored.error());
                    }
                    emit_body_line("mov rax, qword ptr [rbp - " +
                                   std::to_string(*current_function_->aggregate_return_pointer_offset) +
                                   "]");
                    emit_body_line("jmp " + current_function_->exit_label);
                    return {};
                }

                //обычный scalar-return: вычислили значение и нормализовали его
                auto emitted = emit_expression(*return_stmt->value);
                if (!emitted) {
                    return std::unexpected(emitted.error());
                }
                auto normalized = normalize_rax(current_function_->return_type, statement.range);
                if (!normalized) {
                    return std::unexpected(normalized.error());
                }
            }
            emit_body_line("jmp " + current_function_->exit_label);
            return {};
        }

        //break и continue просто прыгают на заранее сохранённые label'ы
        if (dynamic_cast<const AST::BreakStmt*>(&statement) != nullptr) {
            emit_body_line("jmp " + current_function_->break_labels.back());
            return {};
        }

        if (dynamic_cast<const AST::ContinueStmt*>(&statement) != nullptr) {
            emit_body_line("jmp " + current_function_->continue_labels.back());
            return {};
        }

        //инструкция-выражение: либо scalar expression, либо aggregate context
        if (const auto* expression_stmt = dynamic_cast<const AST::ExprStmt*>(&statement)) {
            auto type = expression_type(*expression_stmt->expression);
            if (!type) {
                return std::unexpected(type.error());
            }

            if (type->name == "void" || is_scalar_runtime_type(*type)) {
                return emit_expression(*expression_stmt->expression);
            }

            return emit_aggregate_source_address(*expression_stmt->expression);
        }

        if (dynamic_cast<const AST::EmptyStmt*>(&statement) != nullptr) {
            return {};
        }

        return std::unexpected(make_error(statement.range, "unsupported statement in codegen"));
    }

    //генерация выражения
    //по контракту результат scalar-выражения обычно оказывается в rax
    std::expected<void, CodegenError> emit_expression(const AST::Expr& expression) {
        auto type = expression_type(expression);
        if (!type) {
            return std::unexpected(type.error());
        }

        //чтение локальной scalar-переменной
        if (const auto* identifier = dynamic_cast<const AST::IdentifierExpr*>(&expression)) {
            if (!is_scalar_runtime_type(*type)) {
                return std::unexpected(make_error(
                    identifier->range,
                    "aggregate values cannot be used directly as scalar expressions"));
            }

            auto address = emit_lvalue_address(*identifier);
            if (!address) {
                return std::unexpected(address.error());
            }
            emit_body_line("mov rax, qword ptr [rax]");
            return normalize_rax(*type, expression.range);
        }

        //целочисленный литерал -> mov rax, <value>
        if (const auto* literal = dynamic_cast<const AST::IntLiteralExpr*>(&expression)) {
            auto parsed = parse_unsigned_integer_literal(literal->value);
            if (!parsed) {
                return std::unexpected(
                    make_error(expression.range, "failed to lower integer literal"));
            }
            emit_body_line("mov rax, " + std::to_string(*parsed));
            return normalize_rax(*type, expression.range);
        }

        //bool literal -> 0 или 1
        if (const auto* literal = dynamic_cast<const AST::BoolLiteralExpr*>(&expression)) {
            emit_body_line(std::string("mov rax, ") + (literal->value ? "1" : "0"));
            return {};
        }

        //string literal -> адрес строки в .rodata
        if (const auto* literal = dynamic_cast<const AST::StringLiteralExpr*>(&expression)) {
            const auto label = intern_string(decode_string_literal(literal->value));
            emit_body_line("lea rax, [rip + " + label + "]");
            return {};
        }

        //char literal -> его числовой код
        if (const auto* literal = dynamic_cast<const AST::CharLiteralExpr*>(&expression)) {
            auto decoded = decode_char_literal(literal->value);
            if (!decoded) {
                return std::unexpected(
                    make_error(expression.range, "failed to lower character literal"));
            }
            emit_body_line("mov rax, " + std::to_string(*decoded));
            return normalize_rax(*type, expression.range);
        }

        //float literal -> его битовое представление
        if (const auto* literal = dynamic_cast<const AST::FloatLiteralExpr*>(&expression)) {
            if (!is_float_name(type->name)) {
                return std::unexpected(make_error(
                    expression.range, "internal error: floating-point literal has non-float type"));
            }
            return emit_float_literal(*literal, *type);
        }

        //унарные операции
        if (const auto* unary = dynamic_cast<const AST::UnaryExpr*>(&expression)) {
            auto emitted = emit_expression(*unary->operand);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }

            switch (unary->op_type) {
                case Lexer::TokenType::Minus:
                    if (is_float_name(type->name)) {
                        emit_body_line("mov rdi, rax");
                        emit_body_line("call __minic_rt_" + runtime_float_name(*type) + "_neg");
                        return {};
                    }
                    emit_body_line("neg rax");
                    return normalize_rax(*type, expression.range);

                case Lexer::TokenType::Bang:
                    emit_body_line("cmp rax, 0");
                    emit_body_line("sete al");
                    emit_body_line("movzx rax, al");
                    return {};

                default:
                    return std::unexpected(
                        make_error(expression.range, "unsupported unary operator in codegen"));
            }
        }

        //бинарные операции вынесены отдельно, потому что это большой блок логики
        if (const auto* binary = dynamic_cast<const AST::BinaryExpr*>(&expression)) {
            return emit_binary_expression(*binary);
        }

        //cast<T>(expr)
        if (const auto* cast = dynamic_cast<const AST::CastExpr*>(&expression)) {
            auto source_type = expression_type(*cast->expression);
            if (!source_type) {
                return std::unexpected(source_type.error());
            }

            if (type->name == "string") {
                return emit_string_cast_expression(*cast->expression, *source_type);
            }

            return emit_numeric_cast_expression(*cast->expression, *source_type, *type);
        }

        //вызов функции или builtin
        if (const auto* call = dynamic_cast<const AST::CallExpr*>(&expression)) {
            return emit_call_expression(*call);
        }

        //assignment как выражение:
        //сначала вычисляем адрес слева, потом значение справа и записываем его
        if (const auto* assignment = dynamic_cast<const AST::AssignmentExpr*>(&expression)) {
            if (!is_scalar_runtime_type(*type)) {
                return std::unexpected(make_error(
                    expression.range,
                    "aggregate assignment values cannot be used directly as scalar expressions"));
            }

            const int target_slot = acquire_temp_slot();
            auto target_address = emit_lvalue_address(*assignment->target);
            if (!target_address) {
                release_temp_slot();
                return std::unexpected(target_address.error());
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(target_slot) + "], rax");

            auto emitted = emit_expression(*assignment->value);
            if (!emitted) {
                release_temp_slot();
                return std::unexpected(emitted.error());
            }

            auto normalized = normalize_rax(*type, assignment->value->range);
            if (!normalized) {
                release_temp_slot();
                return std::unexpected(normalized.error());
            }

            emit_body_line("mov rcx, qword ptr [rbp - " + std::to_string(target_slot) + "]");
            emit_body_line("mov qword ptr [rcx], rax");
            release_temp_slot();
            return {};
        }

        //чтение scalar-элемента массива
        if (const auto* index = dynamic_cast<const AST::IndexExpr*>(&expression)) {
            if (!is_scalar_runtime_type(*type)) {
                return std::unexpected(make_error(
                    expression.range,
                    "aggregate array elements require an address context"));
            }

            auto address = emit_lvalue_address(*index);
            if (!address) {
                return std::unexpected(address.error());
            }
            emit_body_line("mov rax, qword ptr [rax]");
            return normalize_rax(*type, expression.range);
        }

        //чтение scalar-поля структуры
        if (const auto* field = dynamic_cast<const AST::FieldAccessExpr*>(&expression)) {
            if (!is_scalar_runtime_type(*type)) {
                return std::unexpected(make_error(
                    expression.range,
                    "aggregate struct fields require an address context"));
            }

            auto address = emit_lvalue_address(*field);
            if (!address) {
                return std::unexpected(address.error());
            }
            emit_body_line("mov rax, qword ptr [rax]");
            return normalize_rax(*type, expression.range);
        }

        if (dynamic_cast<const AST::NamespaceAccessExpr*>(&expression) != nullptr) {
            return std::unexpected(
                make_error(expression.range, "namespace access cannot be used as a value directly"));
        }

        if (dynamic_cast<const AST::ArrayLiteralExpr*>(&expression) != nullptr ||
            dynamic_cast<const AST::StructLiteralExpr*>(&expression) != nullptr) {
            return std::unexpected(make_error(
                expression.range,
                "aggregate literals require an address context in codegen"));
        }

        return std::unexpected(make_error(expression.range, "unsupported expression in codegen"));
    }

    //генерация бинарных операторов
    //сюда попадает и arithmetic, и comparisons, и short-circuit логика
    std::expected<void, CodegenError> emit_binary_expression(const AST::BinaryExpr& binary) {
        //логическое И с short-circuit
        if (binary.op_type == Lexer::TokenType::AmpAmp) {
            const auto false_label = new_label(".Land_false");
            const auto end_label = new_label(".Land_end");

            auto left = emit_expression(*binary.left);
            if (!left) {
                return std::unexpected(left.error());
            }
            emit_body_line("cmp rax, 0");
            emit_body_line("je " + false_label);

            auto right = emit_expression(*binary.right);
            if (!right) {
                return std::unexpected(right.error());
            }
            emit_body_line("cmp rax, 0");
            emit_body_line("setne al");
            emit_body_line("movzx rax, al");
            emit_body_line("jmp " + end_label);
            emit_body_label(false_label);
            emit_body_line("mov rax, 0");
            emit_body_label(end_label);
            return {};
        }

        //логическое ИЛИ с short-circuit
        if (binary.op_type == Lexer::TokenType::PipePipe) {
            const auto true_label = new_label(".Lor_true");
            const auto end_label = new_label(".Lor_end");

            auto left = emit_expression(*binary.left);
            if (!left) {
                return std::unexpected(left.error());
            }
            emit_body_line("cmp rax, 0");
            emit_body_line("jne " + true_label);

            auto right = emit_expression(*binary.right);
            if (!right) {
                return std::unexpected(right.error());
            }
            emit_body_line("cmp rax, 0");
            emit_body_line("setne al");
            emit_body_line("movzx rax, al");
            emit_body_line("jmp " + end_label);
            emit_body_label(true_label);
            emit_body_line("mov rax, 1");
            emit_body_label(end_label);
            return {};
        }

        auto left_type = expression_type(*binary.left);
        if (!left_type) {
            return std::unexpected(left_type.error());
        }
        auto right_type = expression_type(*binary.right);
        if (!right_type) {
            return std::unexpected(right_type.error());
        }
        auto result_type = expression_type(binary);
        if (!result_type) {
            return std::unexpected(result_type.error());
        }

        if ((binary.op_type == Lexer::TokenType::EqualEqual ||
             binary.op_type == Lexer::TokenType::BangEqual) &&
            is_aggregate_runtime_type(*left_type)) {
            //для arrays/structs equality считается не через cmp rax, rcx,
            //а через отдельный поэлементный обход
            const int left_slot = acquire_temp_slot();
            const int right_slot = acquire_temp_slot();

            auto left_address = emit_aggregate_source_address(*binary.left);
            if (!left_address) {
                release_temp_slot();
                release_temp_slot();
                return std::unexpected(left_address.error());
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(left_slot) + "], rax");

            auto right_address = emit_aggregate_source_address(*binary.right);
            if (!right_address) {
                release_temp_slot();
                release_temp_slot();
                return std::unexpected(right_address.error());
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(right_slot) + "], rax");

            auto compared =
                emit_aggregate_equality_result(*left_type, left_slot, right_slot, binary.range);
            release_temp_slot();
            release_temp_slot();
            if (!compared) {
                return std::unexpected(compared.error());
            }

            if (binary.op_type == Lexer::TokenType::BangEqual) {
                emit_body_line("xor rax, 1");
            }
            return {};
        }

        const int temp_offset = acquire_temp_slot();
        //обычная схема для scalar binary expression:
        //левый операнд временно сохраняем в стек,
        //правый кладём в rcx, левый возвращаем в rax
        auto left = emit_expression(*binary.left);
        if (!left) {
            release_temp_slot();
            return std::unexpected(left.error());
        }
        emit_body_line("mov qword ptr [rbp - " + std::to_string(temp_offset) + "], rax");

        auto right = emit_expression(*binary.right);
        if (!right) {
            release_temp_slot();
            return std::unexpected(right.error());
        }
        emit_body_line("mov rcx, rax");
        emit_body_line("mov rax, qword ptr [rbp - " + std::to_string(temp_offset) + "]");
        release_temp_slot();

        if (binary.op_type == Lexer::TokenType::Plus && left_type->name == "string" &&
            right_type->name == "string") {
            //конкатенация строк уходит в runtime helper
            emit_body_line("mov rdi, rax");
            emit_body_line("mov rsi, rcx");
            emit_body_line("call __minic_rt_concat");
            return {};
        }

        if (is_float_name(left_type->name)) {
            //для float-операций используем runtime helpers
            return emit_float_binary_expression(binary, *left_type);
        }

        switch (binary.op_type) {
            case Lexer::TokenType::Plus:
                emit_body_line("add rax, rcx");
                return normalize_rax(*result_type, binary.range);

            case Lexer::TokenType::Minus:
                emit_body_line("sub rax, rcx");
                return normalize_rax(*result_type, binary.range);

            case Lexer::TokenType::Star:
                emit_body_line("imul rax, rcx");
                return normalize_rax(*result_type, binary.range);

            case Lexer::TokenType::Slash:
            case Lexer::TokenType::Percent: {
                const auto non_zero = new_label(".Ldiv_ok");
                emit_body_line("cmp rcx, 0");
                emit_body_line("jne " + non_zero);
                emit_body_line("mov rdi, " +
                               std::to_string(binary.right->range.begin.line));
                emit_body_line("call __minic_rt_div_zero");
                emit_body_label(non_zero);

                if (is_unsigned_integer_name(left_type->name)) {
                    emit_body_line("xor edx, edx");
                    emit_body_line("div rcx");
                } else {
                    emit_body_line("cqo");
                    emit_body_line("idiv rcx");
                }

                if (binary.op_type == Lexer::TokenType::Percent) {
                    emit_body_line("mov rax, rdx");
                }
                return normalize_rax(*result_type, binary.range);
            }

            case Lexer::TokenType::EqualEqual:
            case Lexer::TokenType::BangEqual:
                if (left_type->name == "string") {
                    emit_body_line("mov rdi, rax");
                    emit_body_line("mov rsi, rcx");
                    emit_body_line("call strcmp");
                    emit_body_line("cmp rax, 0");
                } else {
                    emit_body_line("cmp rax, rcx");
                }
                if (binary.op_type == Lexer::TokenType::EqualEqual) {
                    emit_body_line("sete al");
                } else {
                    emit_body_line("setne al");
                }
                emit_body_line("movzx rax, al");
                return {};

            case Lexer::TokenType::Less:
            case Lexer::TokenType::LessEqual:
            case Lexer::TokenType::Greater:
            case Lexer::TokenType::GreaterEqual:
                emit_body_line("cmp rax, rcx");
                emit_body_line(setcc_instruction(binary.op_type, left_type->name) + " al");
                emit_body_line("movzx rax, al");
                return {};

            default:
                return std::unexpected(
                    make_error(binary.range, "unsupported binary operator in codegen"));
        }
    }

    //сравнение двух scalar-значений на равенство
    //в зависимости от типа выбирается разный путь: strcmp, runtime helper или обычный cmp
    std::expected<void, CodegenError> emit_scalar_equality_result(
        const Semantic::SemanticType& type, std::string_view left_reg,
        std::string_view right_reg, const Lexer::SourceRange& range) {
        if (type.name == "string") {
            emit_body_line("mov rdi, " + std::string(left_reg));
            emit_body_line("mov rsi, " + std::string(right_reg));
            emit_body_line("call strcmp");
            emit_body_line("cmp rax, 0");
            emit_body_line("sete al");
            emit_body_line("movzx rax, al");
            return {};
        }

        if (is_float_name(type.name)) {
            emit_body_line("mov rdi, " + std::string(left_reg));
            emit_body_line("mov rsi, " + std::string(right_reg));
            emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_eq");
            return {};
        }

        if (type.name == "bool" || type.name == "char" || is_integer_name(type.name)) {
            emit_body_line("cmp " + std::string(left_reg) + ", " + std::string(right_reg));
            emit_body_line("sete al");
            emit_body_line("movzx rax, al");
            return {};
        }

        return std::unexpected(
            make_error(range, "codegen does not yet support equality for type '" + type.name + '\''));
    }

    //сравнение значений по двум адресам в памяти
    //если тип scalar — просто читаем значения и сравниваем
    //если aggregate — уходим в более глубокий обход
    std::expected<void, CodegenError> emit_equality_result_from_addresses(
        const Semantic::SemanticType& type, std::string_view left_addr_reg,
        std::string_view right_addr_reg, const Lexer::SourceRange& range) {
        if (type.kind == Semantic::SemanticTypeKind::Builtin) {
            emit_body_line("mov rax, qword ptr [" + std::string(left_addr_reg) + "]");
            emit_body_line("mov rcx, qword ptr [" + std::string(right_addr_reg) + "]");
            return emit_scalar_equality_result(type, "rax", "rcx", range);
        }

        const int left_slot = acquire_temp_slot();
        const int right_slot = acquire_temp_slot();
        emit_body_line("mov qword ptr [rbp - " + std::to_string(left_slot) + "], " +
                       std::string(left_addr_reg));
        emit_body_line("mov qword ptr [rbp - " + std::to_string(right_slot) + "], " +
                       std::string(right_addr_reg));
        auto compared = emit_aggregate_equality_result(type, left_slot, right_slot, range);
        release_temp_slot();
        release_temp_slot();
        return compared;
    }

    //поэлементное сравнение массивов и структур
    std::expected<void, CodegenError> emit_aggregate_equality_result(
        const Semantic::SemanticType& type, int left_slot, int right_slot,
        const Lexer::SourceRange& range) {
        const auto fail_label = new_label(".Leq_fail");
        const auto end_label = new_label(".Leq_end");

        if (type.kind == Semantic::SemanticTypeKind::Array) {
            auto element_type = resolve_type_name(type.element_type_name, range);
            if (!element_type) {
                return std::unexpected(element_type.error());
            }

            auto element_size = type_size(*element_type, range);
            if (!element_size) {
                return std::unexpected(element_size.error());
            }

            for (std::size_t index = 0; index < type.array_size; ++index) {
                const int offset = static_cast<int>(index) * (*element_size);
                emit_body_line("mov rdx, qword ptr [rbp - " + std::to_string(left_slot) + "]");
                if (offset != 0) {
                    emit_body_line("add rdx, " + std::to_string(offset));
                }
                emit_body_line("mov r8, qword ptr [rbp - " + std::to_string(right_slot) + "]");
                if (offset != 0) {
                    emit_body_line("add r8, " + std::to_string(offset));
                }

                auto compared =
                    emit_equality_result_from_addresses(*element_type, "rdx", "r8", range);
                if (!compared) {
                    return std::unexpected(compared.error());
                }
                emit_body_line("cmp rax, 0");
                emit_body_line("je " + fail_label);
            }

            emit_body_line("mov rax, 1");
            emit_body_line("jmp " + end_label);
            emit_body_label(fail_label);
            emit_body_line("mov rax, 0");
            emit_body_label(end_label);
            return {};
        }

        if (type.kind == Semantic::SemanticTypeKind::Struct) {
            auto struct_info = lookup_struct_info(type.name, range);
            if (!struct_info) {
                return std::unexpected(struct_info.error());
            }

            int offset = 0;
            for (const auto& field : (*struct_info)->fields) {
                emit_body_line("mov rdx, qword ptr [rbp - " + std::to_string(left_slot) + "]");
                if (offset != 0) {
                    emit_body_line("add rdx, " + std::to_string(offset));
                }
                emit_body_line("mov r8, qword ptr [rbp - " + std::to_string(right_slot) + "]");
                if (offset != 0) {
                    emit_body_line("add r8, " + std::to_string(offset));
                }

                auto compared = emit_equality_result_from_addresses(field.type, "rdx", "r8", range);
                if (!compared) {
                    return std::unexpected(compared.error());
                }
                emit_body_line("cmp rax, 0");
                emit_body_line("je " + fail_label);

                auto field_size = type_size(field.type, range);
                if (!field_size) {
                    return std::unexpected(field_size.error());
                }
                offset += *field_size;
            }

            emit_body_line("mov rax, 1");
            emit_body_line("jmp " + end_label);
            emit_body_label(fail_label);
            emit_body_line("mov rax, 0");
            emit_body_label(end_label);
            return {};
        }

        return std::unexpected(
            make_error(range, "internal error: aggregate equality requires array or struct type"));
    }

    //имя runtime helper для float-типов
    [[nodiscard]] std::string runtime_float_name(const Semantic::SemanticType& type) const {
        return type.name == "float32" ? "f32" : "f64";
    }

    //имя runtime helper для integer-типов
    [[nodiscard]] std::string runtime_integer_name(const Semantic::SemanticType& type) const {
        return is_unsigned_integer_name(type.name) ? "u64" : "i64";
    }

    //float literal превращаем в его битовый образ,
    //потому что backend хранит float как raw bits в rax/eax
    std::expected<void, CodegenError> emit_float_literal(const AST::FloatLiteralExpr& literal,
                                                         const Semantic::SemanticType& type) {
        try {
            if (type.name == "float32") {
                const float value = std::stof(literal.value);
                const auto bits = std::bit_cast<std::uint32_t>(value);
                emit_body_line("mov eax, " + format_u32_hex(bits));
                return {};
            }

            const double value = std::stod(literal.value);
            const auto bits = std::bit_cast<std::uint64_t>(value);
            emit_body_line("mov rax, " + format_u64_hex(bits));
            return {};
        } catch (const std::exception&) {
            return std::unexpected(
                make_error(literal.range, "failed to lower floating-point literal"));
        }
    }

    //проверка деления float на ноль через runtime helper
    std::expected<void, CodegenError> emit_float_division_zero_check(
        const Semantic::SemanticType& type, const Lexer::SourceRange& range) {
        const auto non_zero = new_label(".Lfdiv_ok");
        emit_body_line("mov rdi, rcx");
        emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_is_zero");
        emit_body_line("cmp rax, 0");
        emit_body_line("je " + non_zero);
        emit_body_line("mov rdi, " + std::to_string(range.begin.line));
        emit_body_line("call __minic_rt_div_zero");
        emit_body_label(non_zero);
        return {};
    }

    //все float-операции lowering'ятся в вызовы runtime
    std::expected<void, CodegenError> emit_float_binary_expression(
        const AST::BinaryExpr& binary, const Semantic::SemanticType& type) {
        if (binary.op_type == Lexer::TokenType::Slash ||
            binary.op_type == Lexer::TokenType::Percent) {
            auto checked = emit_float_division_zero_check(type, binary.right->range);
            if (!checked) {
                return std::unexpected(checked.error());
            }
        }

        emit_body_line("mov rdi, rax");
        emit_body_line("mov rsi, rcx");

        switch (binary.op_type) {
            case Lexer::TokenType::Plus:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_add");
                return {};
            case Lexer::TokenType::Minus:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_sub");
                return {};
            case Lexer::TokenType::Star:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_mul");
                return {};
            case Lexer::TokenType::Slash:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_div");
                return {};
            case Lexer::TokenType::Percent:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_mod");
                return {};
            case Lexer::TokenType::EqualEqual:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_eq");
                return {};
            case Lexer::TokenType::BangEqual:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_ne");
                return {};
            case Lexer::TokenType::Less:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_lt");
                return {};
            case Lexer::TokenType::LessEqual:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_le");
                return {};
            case Lexer::TokenType::Greater:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_gt");
                return {};
            case Lexer::TokenType::GreaterEqual:
                emit_body_line("call __minic_rt_" + runtime_float_name(type) + "_ge");
                return {};
            default:
                return std::unexpected(
                    make_error(binary.range, "unsupported floating-point operator in codegen"));
        }
    }

    //явные numeric cast'ы: int -> float, float -> int, float -> float
    std::expected<void, CodegenError> emit_numeric_cast_expression(
        const AST::Expr& operand, const Semantic::SemanticType& source_type,
        const Semantic::SemanticType& target_type) {
        auto emitted = emit_expression(operand);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }

        if (source_type.name == target_type.name) {
            return normalize_rax(target_type, operand.range);
        }

        if (is_integer_name(source_type.name) && is_integer_name(target_type.name)) {
            return normalize_rax(target_type, operand.range);
        }

        emit_body_line("mov rdi, rax");

        if (is_integer_name(source_type.name) && is_float_name(target_type.name)) {
            emit_body_line("call __minic_rt_" + runtime_integer_name(source_type) + "_to_" +
                           runtime_float_name(target_type));
            return {};
        }

        if (is_float_name(source_type.name) && is_integer_name(target_type.name)) {
            emit_body_line("call __minic_rt_" + runtime_float_name(source_type) + "_to_" +
                           runtime_integer_name(target_type));
            return normalize_rax(target_type, operand.range);
        }

        if (is_float_name(source_type.name) && is_float_name(target_type.name)) {
            emit_body_line("call __minic_rt_" + runtime_float_name(source_type) + "_to_" +
                           runtime_float_name(target_type));
            return {};
        }

        return std::unexpected(make_error(
            operand.range, "unsupported numeric cast from '" + source_type.name + "' to '" +
                               target_type.name + '\''));
    }

    //cast<string>(...)
    //складываем operand в rdi и зовём нужный runtime helper
    std::expected<void, CodegenError> emit_string_cast_expression(
        const AST::Expr& operand, const Semantic::SemanticType& source_type) {
        auto emitted = emit_expression(operand);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }

        emit_body_line("mov rdi, rax");
        if (source_type.name == "bool") {
            emit_body_line("call __minic_rt_bool_to_string");
            return {};
        }
        if (source_type.name == "char") {
            emit_body_line("call __minic_rt_char_to_string");
            return {};
        }
        if (is_integer_name(source_type.name)) {
            emit_body_line("call __minic_rt_" + runtime_integer_name(source_type) + "_to_string");
            return {};
        }
        if (is_float_name(source_type.name)) {
            emit_body_line("call __minic_rt_" + runtime_float_name(source_type) + "_to_string");
            return {};
        }

        return std::unexpected(make_error(
            operand.range, "unsupported cast to 'string' from '" + source_type.name + '\''));
    }

    //вызов функции:
    //сначала проверяем, не builtin ли это,
    //иначе подготавливаем аргументы и делаем обычный call
    std::expected<void, CodegenError> emit_call_expression(const AST::CallExpr& call) {
        const auto resolved = semantic_result_.resolved_functions.find(call.callee.get());
        if (resolved == semantic_result_.resolved_functions.end()) {
            return std::unexpected(
                make_error(call.range, "internal error: unresolved callee in codegen"));
        }

        if (resolved->second == "print") {
            return emit_builtin_print(call);
        }
        if (resolved->second == "len") {
            return emit_builtin_len(call);
        }
        if (resolved->second == "exit") {
            return emit_builtin_exit(call);
        }
        if (resolved->second == "panic") {
            return emit_builtin_panic(call);
        }
        if (resolved->second == "input") {
            return emit_builtin_input(call);
        }
        if (resolved->second == "assert") {
            return emit_builtin_assert(call);
        }

        const auto info_it = functions_by_name_.find(resolved->second);
        if (info_it == functions_by_name_.end()) {
            return std::unexpected(make_error(
                call.range, "internal error: missing function info for '" + resolved->second + '\''));
        }
        const auto* info = info_it->second;
        if (info->parameter_types.size() > 6) {
            return std::unexpected(make_error(
                call.range, "codegen currently supports at most 6 call arguments"));
        }
        if (!is_scalar_runtime_type(info->return_type) && info->return_type.name != "void") {
            return std::unexpected(make_error(
                call.range,
                "aggregate-return function can only be used in an aggregate context"));
        }

        std::vector<int> argument_offsets;
        //вычисляем все аргументы заранее и временно складываем их в стек
        argument_offsets.reserve(call.arguments.size());
        for (std::size_t i = 0; i < call.arguments.size(); ++i) {
            auto supported = ensure_supported_type(info->parameter_types[i], call.arguments[i]->range);
            if (!supported) {
                return std::unexpected(supported.error());
            }

            const int offset = acquire_temp_slot();
            if (is_aggregate_runtime_type(info->parameter_types[i])) {
                auto emitted = emit_aggregate_source_address(*call.arguments[i]);
                if (!emitted) {
                    release_temp_slot();
                    return std::unexpected(emitted.error());
                }
            } else {
                auto emitted = emit_expression(*call.arguments[i]);
                if (!emitted) {
                    release_temp_slot();
                    return std::unexpected(emitted.error());
                }
                auto normalized = normalize_rax(info->parameter_types[i], call.arguments[i]->range);
                if (!normalized) {
                    release_temp_slot();
                    return std::unexpected(normalized.error());
                }
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(offset) + "], rax");
            argument_offsets.push_back(offset);
        }

        for (std::size_t i = 0; i < argument_offsets.size(); ++i) {
            //раскладываем аргументы по ABI-регистрам
            emit_body_line("mov " + std::string(kArgumentRegisters[i]) + ", qword ptr [rbp - " +
                           std::to_string(argument_offsets[i]) + "]");
        }
        for (std::size_t i = 0; i < argument_offsets.size(); ++i) {
            release_temp_slot();
        }

        emit_body_line("call " + mangle_name(resolved->second));
        return normalize_rax(info->return_type, call.range);
    }

    //builtin print
    std::expected<void, CodegenError> emit_builtin_print(const AST::CallExpr& call) {
        if (call.arguments.size() != 1) {
            return std::unexpected(make_error(call.range, "builtin 'print' expects 1 argument"));
        }

        auto argument_type = expression_type(*call.arguments[0]);
        if (!argument_type) {
            return std::unexpected(argument_type.error());
        }
        if (!is_scalar_runtime_type(*argument_type)) {
            return std::unexpected(make_error(
                call.arguments[0]->range,
                "codegen does not yet support printing type '" + argument_type->name + '\''));
        }

        auto emitted = emit_expression(*call.arguments[0]);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }
        emit_body_line("mov rdi, rax");

        if (argument_type->name == "string") {
            emit_body_line("call __minic_rt_print_string");
        } else if (argument_type->name == "bool") {
            emit_body_line("call __minic_rt_print_bool");
        } else if (argument_type->name == "char") {
            emit_body_line("call __minic_rt_print_char");
        } else if (argument_type->name == "float32") {
            emit_body_line("call __minic_rt_print_f32");
        } else if (argument_type->name == "float64") {
            emit_body_line("call __minic_rt_print_f64");
        } else if (is_integer_name(argument_type->name)) {
            emit_body_line("call __minic_rt_" +
                           std::string(is_unsigned_integer_name(argument_type->name)
                                           ? "print_u64"
                                           : "print_i64"));
        } else {
            return std::unexpected(make_error(
                call.arguments[0]->range,
                "codegen does not yet support printing type '" + argument_type->name + '\''));
        }

        emit_body_line("mov rax, 0");
        return {};
    }

    //builtin len
    //для массива длина известна сразу из типа, для строки зовём strlen
    std::expected<void, CodegenError> emit_builtin_len(const AST::CallExpr& call) {
        if (call.arguments.size() != 1) {
            return std::unexpected(make_error(call.range, "builtin 'len' expects 1 argument"));
        }

        auto argument_type = expression_type(*call.arguments[0]);
        if (!argument_type) {
            return std::unexpected(argument_type.error());
        }

        if (argument_type->kind == Semantic::SemanticTypeKind::Array) {
            auto evaluated = emit_aggregate_source_address(*call.arguments[0]);
            if (!evaluated) {
                return std::unexpected(evaluated.error());
            }
            emit_body_line("mov rax, " + std::to_string(argument_type->array_size));
            auto type = expression_type(call);
            if (!type) {
                return std::unexpected(type.error());
            }
            return normalize_rax(*type, call.range);
        }

        auto emitted = emit_expression(*call.arguments[0]);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }
        emit_body_line("mov rdi, rax");
        emit_body_line("call strlen");

        auto type = expression_type(call);
        if (!type) {
            return std::unexpected(type.error());
        }
        return normalize_rax(*type, call.range);
    }

    //builtin assert
    //если условие ложно, падаем через runtime panic
    std::expected<void, CodegenError> emit_builtin_assert(const AST::CallExpr& call) {
        if (call.arguments.size() != 1) {
            return std::unexpected(make_error(call.range, "builtin 'assert' expects 1 argument"));
        }

        auto emitted = emit_expression(*call.arguments[0]);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }

        const auto ok_label = new_label(".Lassert_ok");
        const auto message_label = intern_string("assertion failed");
        emit_body_line("cmp rax, 0");
        emit_body_line("jne " + ok_label);
        emit_body_line("lea rdi, [rip + " + message_label + "]");
        emit_body_line("call __minic_rt_panic");
        emit_body_label(ok_label);
        emit_body_line("mov rax, 0");
        return {};
    }

    std::expected<void, CodegenError> emit_builtin_exit(const AST::CallExpr& call) {
        if (call.arguments.size() != 1) {
            return std::unexpected(make_error(call.range, "builtin 'exit' expects 1 argument"));
        }

        auto emitted = emit_expression(*call.arguments[0]);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }
        emit_body_line("mov rdi, rax");
        emit_body_line("call exit");
        emit_body_line("mov rax, 0");
        return {};
    }

    std::expected<void, CodegenError> emit_builtin_panic(const AST::CallExpr& call) {
        if (call.arguments.size() != 1) {
            return std::unexpected(make_error(call.range, "builtin 'panic' expects 1 argument"));
        }

        auto emitted = emit_expression(*call.arguments[0]);
        if (!emitted) {
            return std::unexpected(emitted.error());
        }
        emit_body_line("mov rdi, rax");
        emit_body_line("call __minic_rt_panic");
        emit_body_line("mov rax, 0");
        return {};
    }

    std::expected<void, CodegenError> emit_builtin_input(const AST::CallExpr& call) {
        if (!call.arguments.empty()) {
            return std::unexpected(make_error(call.range, "builtin 'input' expects no arguments"));
        }

        emit_body_line("call __minic_rt_input");
        return {};
    }

    //записать выражение по адресу в rdi
    //это основной helper для array/struct copy semantics
    std::expected<void, CodegenError> emit_store_expression_to_rdi(
        const AST::Expr& expression, const Semantic::SemanticType& target_type) {
        auto supported = ensure_supported_type(target_type, expression.range);
        if (!supported) {
            return std::unexpected(supported.error());
        }

        if (is_scalar_runtime_type(target_type)) {
            auto emitted = emit_expression(expression);
            if (!emitted) {
                return std::unexpected(emitted.error());
            }
            auto normalized = normalize_rax(target_type, expression.range);
            if (!normalized) {
                return std::unexpected(normalized.error());
            }
            emit_body_line("mov qword ptr [rdi], rax");
            return {};
        }

        if (const auto* array_literal = dynamic_cast<const AST::ArrayLiteralExpr*>(&expression)) {
            return emit_array_literal_to_pointer(*array_literal, target_type);
        }

        if (const auto* struct_literal = dynamic_cast<const AST::StructLiteralExpr*>(&expression)) {
            return emit_struct_literal_to_pointer(*struct_literal, target_type);
        }

        const int destination_slot = acquire_temp_slot();
        emit_body_line("mov qword ptr [rbp - " + std::to_string(destination_slot) + "], rdi");

        auto source = emit_aggregate_source_address(expression);
        if (!source) {
            release_temp_slot();
            return std::unexpected(source.error());
        }

        auto size = type_size(target_type, expression.range);
        if (!size) {
            release_temp_slot();
            return std::unexpected(size.error());
        }

        emit_body_line("mov rsi, rax");
        emit_body_line("mov rdi, qword ptr [rbp - " + std::to_string(destination_slot) + "]");
        emit_body_line("mov rdx, " + std::to_string(*size));
        emit_body_line("call memcpy");
        release_temp_slot();
        return {};
    }

    //запись литерала массива в заранее выделенную память
    std::expected<void, CodegenError> emit_array_literal_to_pointer(
        const AST::ArrayLiteralExpr& literal, const Semantic::SemanticType& target_type) {
        auto element_type = resolve_type_name(target_type.element_type_name, literal.range);
        if (!element_type) {
            return std::unexpected(element_type.error());
        }

        auto element_size = type_size(*element_type, literal.range);
        if (!element_size) {
            return std::unexpected(element_size.error());
        }

        const int base_slot = acquire_temp_slot();
        emit_body_line("mov qword ptr [rbp - " + std::to_string(base_slot) + "], rdi");

        for (std::size_t i = 0; i < literal.elements.size(); ++i) {
            emit_body_line("mov rdi, qword ptr [rbp - " + std::to_string(base_slot) + "]");
            const int byte_offset = static_cast<int>(i) * (*element_size);
            if (byte_offset != 0) {
                emit_body_line("add rdi, " + std::to_string(byte_offset));
            }

            auto stored = emit_store_expression_to_rdi(*literal.elements[i], *element_type);
            if (!stored) {
                release_temp_slot();
                return std::unexpected(stored.error());
            }
        }

        release_temp_slot();
        return {};
    }

    //запись литерала структуры в заранее выделенную память
    std::expected<void, CodegenError> emit_struct_literal_to_pointer(
        const AST::StructLiteralExpr& literal, const Semantic::SemanticType& target_type) {
        auto struct_info = lookup_struct_info(target_type.name, literal.range);
        if (!struct_info) {
            return std::unexpected(struct_info.error());
        }

        std::unordered_map<std::string, const AST::Expr*> initializers;
        for (const auto& field : literal.fields) {
            initializers.emplace(field.name, field.value.get());
        }

        const int base_slot = acquire_temp_slot();
        emit_body_line("mov qword ptr [rbp - " + std::to_string(base_slot) + "], rdi");

        int running_offset = 0;
        for (const auto& field : (*struct_info)->fields) {
            emit_body_line("mov rdi, qword ptr [rbp - " + std::to_string(base_slot) + "]");
            if (running_offset != 0) {
                emit_body_line("add rdi, " + std::to_string(running_offset));
            }

            const auto initializer = initializers.find(field.name);
            if (initializer == initializers.end()) {
                release_temp_slot();
                return std::unexpected(make_error(
                    literal.range, "internal error: missing initializer for field '" +
                                       field.name + '\''));
            }

            auto stored = emit_store_expression_to_rdi(*initializer->second, field.type);
            if (!stored) {
                release_temp_slot();
                return std::unexpected(stored.error());
            }

            auto field_size = type_size(field.type, literal.range);
            if (!field_size) {
                release_temp_slot();
                return std::unexpected(field_size.error());
            }
            running_offset += *field_size;
        }

        release_temp_slot();
        return {};
    }

    //получить адрес aggregate-значения
    //это важно для массивов, структур и aggregate-return функций
    std::expected<void, CodegenError> emit_aggregate_source_address(const AST::Expr& expression) {
        auto type = expression_type(expression);
        if (!type) {
            return std::unexpected(type.error());
        }

        auto supported = ensure_supported_type(*type, expression.range);
        if (!supported) {
            return std::unexpected(supported.error());
        }

        if (is_scalar_runtime_type(*type) || type->name == "void") {
            return std::unexpected(make_error(
                expression.range, "internal error: aggregate address requested for scalar value"));
        }

        if (dynamic_cast<const AST::IdentifierExpr*>(&expression) != nullptr ||
            dynamic_cast<const AST::IndexExpr*>(&expression) != nullptr ||
            dynamic_cast<const AST::FieldAccessExpr*>(&expression) != nullptr) {
            return emit_lvalue_address(expression);
        }

        if (const auto* call = dynamic_cast<const AST::CallExpr*>(&expression)) {
            const auto resolved = semantic_result_.resolved_functions.find(call->callee.get());
            if (resolved == semantic_result_.resolved_functions.end()) {
                return std::unexpected(
                    make_error(call->range, "internal error: unresolved callee in codegen"));
            }
            if (resolved->second == "print" || resolved->second == "len" ||
                resolved->second == "exit" || resolved->second == "panic" ||
                resolved->second == "input") {
                return std::unexpected(make_error(
                    call->range,
                    "internal error: builtin call cannot produce an aggregate value"));
            }

            const auto info_it = functions_by_name_.find(resolved->second);
            if (info_it == functions_by_name_.end()) {
                return std::unexpected(make_error(
                    call->range,
                    "internal error: missing function info for '" + resolved->second + '\''));
            }

            const auto* info = info_it->second;
            if (!is_aggregate_runtime_type(info->return_type)) {
                return std::unexpected(make_error(
                    call->range,
                    "internal error: scalar-return function used in aggregate context"));
            }

            const std::size_t required_registers = info->parameter_types.size() + 1;
            if (required_registers > 6) {
                return std::unexpected(make_error(
                    call->range,
                    "codegen currently supports at most 6 register arguments per call"));
            }

            auto size = type_size(*type, expression.range);
            if (!size) {
                return std::unexpected(size.error());
            }

            const int result_offset = allocate_stack_bytes(*size);
            const int result_pointer_slot = acquire_temp_slot();
            emit_body_line("lea rax, [rbp - " + std::to_string(result_offset) + "]");
            emit_body_line("mov qword ptr [rbp - " + std::to_string(result_pointer_slot) + "], rax");

            std::vector<int> argument_offsets;
            argument_offsets.reserve(call->arguments.size());
            for (std::size_t i = 0; i < call->arguments.size(); ++i) {
                auto supported =
                    ensure_supported_type(info->parameter_types[i], call->arguments[i]->range);
                if (!supported) {
                    release_temp_slot();
                    return std::unexpected(supported.error());
                }

                const int offset = acquire_temp_slot();
                if (is_aggregate_runtime_type(info->parameter_types[i])) {
                    auto emitted = emit_aggregate_source_address(*call->arguments[i]);
                    if (!emitted) {
                        release_temp_slot();
                        release_temp_slot();
                        return std::unexpected(emitted.error());
                    }
                } else {
                    auto emitted = emit_expression(*call->arguments[i]);
                    if (!emitted) {
                        release_temp_slot();
                        release_temp_slot();
                        return std::unexpected(emitted.error());
                    }
                    auto normalized =
                        normalize_rax(info->parameter_types[i], call->arguments[i]->range);
                    if (!normalized) {
                        release_temp_slot();
                        release_temp_slot();
                        return std::unexpected(normalized.error());
                    }
                }
                emit_body_line("mov qword ptr [rbp - " + std::to_string(offset) + "], rax");
                argument_offsets.push_back(offset);
            }

            emit_body_line("mov rdi, qword ptr [rbp - " + std::to_string(result_pointer_slot) + "]");
            for (std::size_t i = 0; i < argument_offsets.size(); ++i) {
                emit_body_line("mov " + std::string(kArgumentRegisters[i + 1]) +
                               ", qword ptr [rbp - " + std::to_string(argument_offsets[i]) + "]");
            }
            for (std::size_t i = 0; i < argument_offsets.size(); ++i) {
                release_temp_slot();
            }
            emit_body_line("call " + mangle_name(resolved->second));
            emit_body_line("mov rax, qword ptr [rbp - " + std::to_string(result_pointer_slot) + "]");
            release_temp_slot();
            return {};
        }

        if (const auto* assignment = dynamic_cast<const AST::AssignmentExpr*>(&expression)) {
            const int target_slot = acquire_temp_slot();
            auto target_address = emit_lvalue_address(*assignment->target);
            if (!target_address) {
                release_temp_slot();
                return std::unexpected(target_address.error());
            }

            emit_body_line("mov qword ptr [rbp - " + std::to_string(target_slot) + "], rax");
            emit_body_line("mov rdi, qword ptr [rbp - " + std::to_string(target_slot) + "]");

            auto stored = emit_store_expression_to_rdi(*assignment->value, *type);
            if (!stored) {
                release_temp_slot();
                return std::unexpected(stored.error());
            }

            emit_body_line("mov rax, qword ptr [rbp - " + std::to_string(target_slot) + "]");
            release_temp_slot();
            return {};
        }

        if (dynamic_cast<const AST::ArrayLiteralExpr*>(&expression) != nullptr ||
            dynamic_cast<const AST::StructLiteralExpr*>(&expression) != nullptr) {
            auto size = type_size(*type, expression.range);
            if (!size) {
                return std::unexpected(size.error());
            }

            const int temp_offset = allocate_stack_bytes(*size);
            emit_body_line("lea rdi, [rbp - " + std::to_string(temp_offset) + "]");
            auto stored = emit_store_expression_to_rdi(expression, *type);
            if (!stored) {
                return std::unexpected(stored.error());
            }
            emit_body_line("lea rax, [rbp - " + std::to_string(temp_offset) + "]");
            return {};
        }

        if (dynamic_cast<const AST::NamespaceAccessExpr*>(&expression) != nullptr) {
            return std::unexpected(make_error(
                expression.range, "namespace access cannot be used as an aggregate value"));
        }

        return std::unexpected(make_error(
            expression.range, "codegen does not yet support this aggregate expression"));
    }

    //получить адрес lvalue:
    //локальной переменной, элемента массива или поля структуры
    std::expected<void, CodegenError> emit_lvalue_address(const AST::Expr& expression) {
        if (const auto* identifier = dynamic_cast<const AST::IdentifierExpr*>(&expression)) {
            auto storage = lookup_storage(identifier->name, expression.range);
            if (!storage) {
                return std::unexpected(storage.error());
            }
            emit_body_line("lea rax, [rbp - " + std::to_string(storage->offset) + "]");
            return {};
        }

        if (const auto* index = dynamic_cast<const AST::IndexExpr*>(&expression)) {
            auto base_type = expression_type(*index->base);
            if (!base_type) {
                return std::unexpected(base_type.error());
            }
            if (base_type->kind != Semantic::SemanticTypeKind::Array) {
                return std::unexpected(make_error(
                    index->base->range, "internal error: index base is not an array"));
            }

            auto element_type = resolve_type_name(base_type->element_type_name, expression.range);
            if (!element_type) {
                return std::unexpected(element_type.error());
            }

            auto element_size = type_size(*element_type, expression.range);
            if (!element_size) {
                return std::unexpected(element_size.error());
            }

            const int base_slot = acquire_temp_slot();
            auto base_address = emit_aggregate_source_address(*index->base);
            if (!base_address) {
                release_temp_slot();
                return std::unexpected(base_address.error());
            }
            emit_body_line("mov qword ptr [rbp - " + std::to_string(base_slot) + "], rax");

            auto index_value = emit_expression(*index->index);
            if (!index_value) {
                release_temp_slot();
                return std::unexpected(index_value.error());
            }

            auto index_type = expression_type(*index->index);
            if (!index_type) {
                release_temp_slot();
                return std::unexpected(index_type.error());
            }

            const auto ok_label = new_label(".Lindex_ok");
            if (!is_unsigned_integer_name(index_type->name)) {
                emit_body_line("cmp rax, 0");
                emit_body_line("jl " + ok_label + "_fail");
            }
            emit_body_line("cmp rax, " + std::to_string(base_type->array_size));
            emit_body_line("jb " + ok_label);
            emit_body_label(ok_label + "_fail");
            emit_body_line("mov rdi, " + std::to_string(index->index->range.begin.line));
            emit_body_line("call __minic_rt_index_out_of_bounds");
            emit_body_label(ok_label);

            if (*element_size != 1) {
                emit_body_line("imul rax, rax, " + std::to_string(*element_size));
            }
            emit_body_line("mov rcx, qword ptr [rbp - " + std::to_string(base_slot) + "]");
            emit_body_line("add rax, rcx");
            release_temp_slot();
            return {};
        }

        if (const auto* field = dynamic_cast<const AST::FieldAccessExpr*>(&expression)) {
            auto base_type = expression_type(*field->base);
            if (!base_type) {
                return std::unexpected(base_type.error());
            }
            if (base_type->kind != Semantic::SemanticTypeKind::Struct) {
                return std::unexpected(make_error(
                    field->base->range, "internal error: field base is not a struct"));
            }

            auto base_address = emit_aggregate_source_address(*field->base);
            if (!base_address) {
                return std::unexpected(base_address.error());
            }

            auto offset = field_offset(*base_type, field->field, expression.range);
            if (!offset) {
                return std::unexpected(offset.error());
            }
            if (*offset != 0) {
                emit_body_line("add rax, " + std::to_string(*offset));
            }
            return {};
        }

        return std::unexpected(make_error(
            expression.range, "codegen does not yet support this assignment target"));
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

        return std::unexpected(make_error(range, "internal error: unresolved local '" + name + '\''));
    }

    std::expected<Semantic::SemanticType, CodegenError> expression_type(
        const AST::Expr& expression) const {
        const auto it = semantic_result_.expr_types.find(&expression);
        if (it == semantic_result_.expr_types.end()) {
            return std::unexpected(make_error(
                expression.range, "internal error: missing expression type annotation"));
        }
        return it->second;
    }

    std::expected<Semantic::SemanticType, CodegenError> lookup_variable_type(
        const AST::VariableDeclStmt& declaration) const {
        const auto it = semantic_result_.variables.find(&declaration);
        if (it == semantic_result_.variables.end()) {
            return std::unexpected(make_error(
                declaration.range, "internal error: missing variable type annotation"));
        }
        return it->second.type;
    }

    //проверяет, умеет ли backend вообще работать с этим типом
    std::expected<void, CodegenError> ensure_supported_type(
        const Semantic::SemanticType& type, const Lexer::SourceRange& range) const {
        switch (type.kind) {
            case Semantic::SemanticTypeKind::Builtin:
                if (type.name == "void" || type.name == "bool" || type.name == "string" ||
                    type.name == "char" || is_integer_name(type.name) || is_float_name(type.name)) {
                    return {};
                }
                return std::unexpected(make_error(
                    range, "codegen does not recognize type '" + type.name + '\''));

            case Semantic::SemanticTypeKind::Array: {
                auto element_type = resolve_type_name(type.element_type_name, range);
                if (!element_type) {
                    return std::unexpected(element_type.error());
                }
                if (element_type->name == "void") {
                    return std::unexpected(make_error(
                        range, "arrays of type 'void' are not supported"));
                }
                return ensure_supported_type(*element_type, range);
            }

            case Semantic::SemanticTypeKind::Struct: {
                auto struct_info = lookup_struct_info(type.name, range);
                if (!struct_info) {
                    return std::unexpected(struct_info.error());
                }
                for (const auto& field : (*struct_info)->fields) {
                    auto supported = ensure_supported_type(field.type, range);
                    if (!supported) {
                        return std::unexpected(supported.error());
                    }
                }
                return {};
            }
        }

        return std::unexpected(make_error(range, "internal error: unsupported semantic type kind"));
    }

    //scalar runtime type = то, что можно носить как одно значение в регистрах / слотах по 8 байт
    [[nodiscard]] bool is_scalar_runtime_type(const Semantic::SemanticType& type) const {
        return type.kind == Semantic::SemanticTypeKind::Builtin &&
               (type.name == "bool" || type.name == "char" || type.name == "string" ||
                is_integer_name(type.name) ||
                is_float_name(type.name));
    }

    //aggregate runtime type = массив или структура
    [[nodiscard]] bool is_aggregate_runtime_type(const Semantic::SemanticType& type) const {
        return type.kind == Semantic::SemanticTypeKind::Array ||
               type.kind == Semantic::SemanticTypeKind::Struct;
    }

    std::expected<Semantic::SemanticType, CodegenError> resolve_type_name(
        std::string_view name, const Lexer::SourceRange& range) const {
        if (name == "void" || name == "bool" || name == "char" || name == "string" ||
            is_integer_name(name) ||
            is_float_name(name)) {
            return Semantic::SemanticType {
                .kind = Semantic::SemanticTypeKind::Builtin,
                .name = std::string(name),
            };
        }

        if (!name.empty() && name.back() == ']') {
            const auto open = name.rfind('[');
            if (open == std::string_view::npos || open + 1 >= name.size() - 1) {
                return std::unexpected(make_error(
                    range, "internal error: malformed array type name '" + std::string(name) + '\''));
            }

            const auto element_name = name.substr(0, open);
            const auto size_text = name.substr(open + 1, name.size() - open - 2);

            const auto parsed = parse_unsigned_integer_literal(size_text);
            if (!parsed) {
                return std::unexpected(make_error(
                    range, "internal error: malformed array size in type '" +
                               std::string(name) + '\''));
            }

            auto element_type = resolve_type_name(element_name, range);
            if (!element_type) {
                return std::unexpected(element_type.error());
            }

            return Semantic::SemanticType {
                .kind = Semantic::SemanticTypeKind::Array,
                .name = std::string(name),
                .element_type_name = element_type->name,
                .array_size = *parsed,
            };
        }

        if (structs_by_name_.contains(std::string(name))) {
            return Semantic::SemanticType {
                .kind = Semantic::SemanticTypeKind::Struct,
                .name = std::string(name),
            };
        }

        return std::unexpected(make_error(
            range, "internal error: unknown type name '" + std::string(name) + '\''));
    }

    std::expected<const Semantic::StructInfo*, CodegenError> lookup_struct_info(
        std::string_view name, const Lexer::SourceRange& range) const {
        const auto it = structs_by_name_.find(std::string(name));
        if (it == structs_by_name_.end()) {
            return std::unexpected(make_error(
                range, "internal error: missing struct info for '" + std::string(name) + '\''));
        }
        return it->second;
    }

    //вычисляет размер типа в байтах
    std::expected<int, CodegenError> type_size(const Semantic::SemanticType& type,
                                               const Lexer::SourceRange& range) const {
        switch (type.kind) {
            case Semantic::SemanticTypeKind::Builtin:
                if (type.name == "bool" || type.name == "char" || type.name == "string" ||
                    is_integer_name(type.name) ||
                    is_float_name(type.name)) {
                    return 8;
                }
                if (type.name == "void") {
                    return std::unexpected(make_error(
                        range, "internal error: 'void' does not have a storage size"));
                }
                return std::unexpected(make_error(
                    range, "codegen does not yet support type '" + type.name + '\''));

            case Semantic::SemanticTypeKind::Array: {
                auto element_type = resolve_type_name(type.element_type_name, range);
                if (!element_type) {
                    return std::unexpected(element_type.error());
                }
                auto element_size = type_size(*element_type, range);
                if (!element_size) {
                    return std::unexpected(element_size.error());
                }
                return (*element_size) * static_cast<int>(type.array_size);
            }

            case Semantic::SemanticTypeKind::Struct: {
                auto struct_info = lookup_struct_info(type.name, range);
                if (!struct_info) {
                    return std::unexpected(struct_info.error());
                }

                int total_size = 0;
                for (const auto& field : (*struct_info)->fields) {
                    auto field_size = type_size(field.type, range);
                    if (!field_size) {
                        return std::unexpected(field_size.error());
                    }
                    total_size += *field_size;
                }
                return align_to(total_size, 8);
            }
        }

        return std::unexpected(make_error(range, "internal error: unsupported semantic type kind"));
    }

    std::expected<int, CodegenError> field_offset(const Semantic::SemanticType& base_type,
                                                  const std::string& field_name,
                                                  const Lexer::SourceRange& range) const {
        auto struct_info = lookup_struct_info(base_type.name, range);
        if (!struct_info) {
            return std::unexpected(struct_info.error());
        }

        int offset = 0;
        for (const auto& field : (*struct_info)->fields) {
            if (field.name == field_name) {
                return offset;
            }

            auto field_size = type_size(field.type, range);
            if (!field_size) {
                return std::unexpected(field_size.error());
            }
            offset += *field_size;
        }

        return std::unexpected(make_error(
            range, "internal error: struct '" + base_type.name + "' has no field '" + field_name +
                       '\''));
    }

    //после вычисления значения в rax приводим его к правильной ширине
    //например int8 -> sign extend, uint8 -> zero extend
    std::expected<void, CodegenError> normalize_rax(const Semantic::SemanticType& type,
                                                    const Lexer::SourceRange& range) {
        if (type.name == "void" || type.name == "string" || type.name == "bool" ||
            is_float_name(type.name)) {
            return {};
        }

        if (type.name == "char") {
            emit_body_line("movzx eax, al");
            return {};
        }

        if (!is_integer_name(type.name)) {
            return std::unexpected(make_error(
                range, "codegen does not yet support type '" + type.name + '\''));
        }

        if (type.name == "int8") {
            emit_body_line("movsx rax, al");
        } else if (type.name == "uint8") {
            emit_body_line("movzx eax, al");
        } else if (type.name == "int16") {
            emit_body_line("movsx rax, ax");
        } else if (type.name == "uint16") {
            emit_body_line("movzx eax, ax");
        } else if (type.name == "int32") {
            emit_body_line("movsxd rax, eax");
        } else if (type.name == "uint32") {
            emit_body_line("mov eax, eax");
        }

        return {};
    }

    [[nodiscard]] std::string setcc_instruction(Lexer::TokenType op_type,
                                                std::string_view operand_type) const {
        const bool is_unsigned = is_unsigned_integer_name(operand_type);

        switch (op_type) {
            case Lexer::TokenType::Less:
                return is_unsigned ? "setb" : "setl";
            case Lexer::TokenType::LessEqual:
                return is_unsigned ? "setbe" : "setle";
            case Lexer::TokenType::Greater:
                return is_unsigned ? "seta" : "setg";
            case Lexer::TokenType::GreaterEqual:
                return is_unsigned ? "setae" : "setge";
            default:
                return "sete";
        }
    }

    //генерация уникального label
    [[nodiscard]] std::string new_label(std::string prefix) {
        return prefix + "_" + std::to_string(label_counter_++);
    }

    //выделяет один 8-байтовый слот на стеке
    [[nodiscard]] int allocate_stack_slot() {
        current_function_->next_stack_offset += 8;
        return current_function_->next_stack_offset;
    }

    //выделяет произвольный блок памяти на стеке с выравниванием
    [[nodiscard]] int allocate_stack_bytes(int size) {
        current_function_->next_stack_offset += align_to(size, 8);
        return current_function_->next_stack_offset;
    }

    //выдаёт временный слот для промежуточных вычислений
    [[nodiscard]] int acquire_temp_slot() {
        if (current_function_->temp_depth == static_cast<int>(current_function_->temp_slots.size())) {
            current_function_->temp_slots.push_back(allocate_stack_slot());
        }
        const int offset = current_function_->temp_slots[current_function_->temp_depth];
        ++current_function_->temp_depth;
        return offset;
    }

    void release_temp_slot() { --current_function_->temp_depth; }

    //добавляет одну инструкцию в тело текущей функции
    void emit_body_line(const std::string& line) { current_function_->body << "    " << line << '\n'; }

    //добавляет label без отступа
    void emit_body_label(const std::string& label) { current_function_->body << label << ":\n"; }

    //кладёт строку в .rodata и возвращает label на неё
    std::string intern_string(const std::string& value) {
        const auto found = string_labels_.find(value);
        if (found != string_labels_.end()) {
            return found->second;
        }

        const auto label = ".Lstr_" + std::to_string(string_counter_++);
        string_labels_.emplace(value, label);
        rodata_ << label << ":\n";
        rodata_ << "    .asciz \"" << escape_asm_string(value) << "\"\n";
        return label;
    }
};

//внешняя точка входа: создаём Generator и просим его вернуть готовый asm
std::expected<std::string, CodegenError> generate_program(
    const AST::Program& program, const Semantic::SemanticResult& semantic_result,
    std::string filename) {
    Generator generator(program, semantic_result, std::move(filename));
    return generator.generate();
}

}
