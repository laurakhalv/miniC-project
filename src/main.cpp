#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <cstdlib>
#include <chrono>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#ifndef MINIC_RUNTIME_PATH
#define MINIC_RUNTIME_PATH "runtime/minic_runtime.s"
#endif

namespace {

struct FrontendError {
    std::string source {};
    std::string message {};
    std::optional<Lexer::SourceLocation> location;
};

std::expected<std::string, std::string> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected("failed to open source file");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void print_tokens(const std::vector<Lexer::Token>& tokens) {
    for (const auto& token : tokens) {
        std::cout << Lexer::to_string(token.type) << " \"" << token.lexeme << "\" "
                  << '@' << token.range.begin.line << ':' << token.range.begin.column << '\n';
    }
}

bool write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output << contents;
    return static_cast<bool>(output);
}

std::string join_module_name(const std::vector<std::string>& parts) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += '.';
        }
        result += parts[i];
    }
    return result;
}

std::string join_identifier_path(const std::vector<std::string>& parts) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += "::";
        }
        result += parts[i];
    }
    return result;
}

std::string join_import_name(const AST::ImportSpec& import) {
    std::string result = join_module_name(import.module_path);
    if (import.imported_path.has_value()) {
        result += "::" + join_identifier_path(*import.imported_path);
    }
    return result;
}

Lexer::SourceRange range_for_declarations(const std::vector<std::unique_ptr<AST::Decl>>& declarations) {
    if (declarations.empty()) {
        return Lexer::SourceRange {};
    }
    return Lexer::SourceRange {
        .begin = declarations.front()->range.begin,
        .end = declarations.back()->range.end,
    };
}

std::vector<std::unique_ptr<AST::Decl>> wrap_in_module_path(
    const std::vector<std::string>& module_path,
    std::vector<std::unique_ptr<AST::Decl>> declarations) {
    if (module_path.empty()) {
        return declarations;
    }

    auto current = std::move(declarations);
    for (std::size_t index = module_path.size(); index-- > 0;) {
        auto range = range_for_declarations(current);
        std::vector<std::unique_ptr<AST::Decl>> wrapped;
        wrapped.push_back(std::make_unique<AST::NamespaceDecl>(
            range, module_path[index], std::move(current)));
        current = std::move(wrapped);
    }
    return current;
}

const std::string* declaration_name(const AST::Decl& declaration) {
    if (const auto* function = dynamic_cast<const AST::FunctionDecl*>(&declaration)) {
        return &function->name;
    }
    if (const auto* structure = dynamic_cast<const AST::StructDecl*>(&declaration)) {
        return &structure->name;
    }
    if (const auto* alias = dynamic_cast<const AST::TypeAliasDecl*>(&declaration)) {
        return &alias->name;
    }
    if (const auto* namespace_decl = dynamic_cast<const AST::NamespaceDecl*>(&declaration)) {
        return &namespace_decl->name;
    }
    return nullptr;
}

std::vector<std::unique_ptr<AST::Decl>> filter_exported_declarations(
    std::vector<std::unique_ptr<AST::Decl>> declarations) {
    std::vector<std::unique_ptr<AST::Decl>> filtered;

    for (auto& declaration : declarations) {
        if (!declaration || !declaration->is_exported) {
            continue;
        }

        if (auto* namespace_decl = dynamic_cast<AST::NamespaceDecl*>(declaration.get())) {
            namespace_decl->declarations =
                filter_exported_declarations(std::move(namespace_decl->declarations));
        }

        filtered.push_back(std::move(declaration));
    }

    return filtered;
}

std::vector<std::unique_ptr<AST::Decl>> extract_exported_declarations(
    std::vector<std::unique_ptr<AST::Decl>>& declarations, const std::vector<std::string>& path,
    std::size_t index = 0) {
    std::vector<std::unique_ptr<AST::Decl>> extracted;
    if (index >= path.size()) {
        return extracted;
    }

    if (index + 1 == path.size()) {
        for (auto& declaration : declarations) {
            if (!declaration || !declaration->is_exported) {
                continue;
            }

            const auto* name = declaration_name(*declaration);
            if (name == nullptr || *name != path[index]) {
                continue;
            }

            if (auto* namespace_decl = dynamic_cast<AST::NamespaceDecl*>(declaration.get())) {
                namespace_decl->declarations =
                    filter_exported_declarations(std::move(namespace_decl->declarations));
            }

            extracted.push_back(std::move(declaration));
        }
        return extracted;
    }

    for (auto& declaration : declarations) {
        if (!declaration || !declaration->is_exported) {
            continue;
        }

        const auto* name = declaration_name(*declaration);
        if (name == nullptr || *name != path[index]) {
            continue;
        }

        auto* namespace_decl = dynamic_cast<AST::NamespaceDecl*>(declaration.get());
        if (namespace_decl == nullptr) {
            continue;
        }

        auto nested =
            extract_exported_declarations(namespace_decl->declarations, path, index + 1);
        if (!nested.empty()) {
            return nested;
        }
    }

    return extracted;
}

std::expected<AST::Program, FrontendError> parse_program_from_file(
    const std::filesystem::path& path) {
    auto source_or_error = read_file(path);
    if (!source_or_error) {
        return std::unexpected(FrontendError {
            .source = {},
            .message = path.string() + ": error: " + source_or_error.error(),
            .location = std::nullopt,
        });
    }

    Lexer::Lexer lexer(*source_or_error, path.string());
    auto tokens_or_error = lexer.tokenize();
    if (!tokens_or_error) {
        return std::unexpected(FrontendError {
            .source = *source_or_error,
            .message = Lexer::format_error(tokens_or_error.error()),
            .location = tokens_or_error.error().location,
        });
    }

    Parser::Parser parser(std::move(*tokens_or_error), path.string());
    auto program_or_error = parser.parse_program();
    if (!program_or_error) {
        return std::unexpected(FrontendError {
            .source = *source_or_error,
            .message = Parser::format_error(program_or_error.error()),
            .location = program_or_error.error().location,
        });
    }

    return std::move(*program_or_error);
}

std::filesystem::path resolve_module_path(const std::filesystem::path& base_dir,
                                          const std::vector<std::string>& module_name) {
    std::filesystem::path path = base_dir;
    for (const auto& part : module_name) {
        path /= part;
    }
    path += ".mc";
    return path;
}

std::expected<std::vector<std::unique_ptr<AST::Decl>>, FrontendError> load_imported_module_decls(
    const std::filesystem::path& root_dir, const AST::ImportSpec& import_spec,
    std::unordered_set<std::string>& loaded_imports) {
    const auto key = join_import_name(import_spec);
    if (!loaded_imports.insert(key).second) {
        return std::vector<std::unique_ptr<AST::Decl>> {};
    }

    const auto module_path = resolve_module_path(root_dir, import_spec.module_path);
    auto program_or_error = parse_program_from_file(module_path);
    if (!program_or_error) {
        return std::unexpected(program_or_error.error());
    }

    if (program_or_error->module_name.has_value() &&
        *program_or_error->module_name != import_spec.module_path) {
        return std::unexpected(FrontendError {
            .source = {},
            .message = module_path.string() + ": error: module name does not match import '" +
                       key + '\'',
            .location = std::nullopt,
        });
    }

    std::vector<std::unique_ptr<AST::Decl>> declarations;
    for (const auto& nested_import : program_or_error->imports) {
        auto imported = load_imported_module_decls(module_path.parent_path(), nested_import,
                                                   loaded_imports);
        if (!imported) {
            return std::unexpected(imported.error());
        }
        for (auto& declaration : imported.value()) {
            declarations.push_back(std::move(declaration));
        }
    }

    if (import_spec.imported_path.has_value()) {
        auto extracted = extract_exported_declarations(program_or_error->declarations,
                                                       *import_spec.imported_path);
        if (extracted.empty()) {
            return std::unexpected(FrontendError {
                .source = {},
                .message = module_path.string() + ": error: exported declaration '" + key +
                           "' was not found",
                .location = std::nullopt,
            });
        }
        for (auto& declaration : extracted) {
            declarations.push_back(std::move(declaration));
        }
        return declarations;
    }

    auto exported = filter_exported_declarations(std::move(program_or_error->declarations));
    auto wrapped = wrap_in_module_path(import_spec.module_path, std::move(exported));
    for (auto& declaration : wrapped) {
        declarations.push_back(std::move(declaration));
    }
    return declarations;
}

std::expected<AST::Program, FrontendError> expand_program_imports(
    const std::filesystem::path& root_path, AST::Program root_program) {
    if (root_program.imports.empty()) {
        return std::move(root_program);
    }

    std::unordered_set<std::string> loaded_imports;
    AST::Program combined;
    combined.module_name = root_program.module_name;
    combined.imports = root_program.imports;

    for (const auto& import_spec : root_program.imports) {
        auto imported =
            load_imported_module_decls(root_path.parent_path(), import_spec, loaded_imports);
        if (!imported) {
            return std::unexpected(imported.error());
        }
        for (auto& declaration : imported.value()) {
            combined.declarations.push_back(std::move(declaration));
        }
    }

    for (auto& declaration : root_program.declarations) {
        combined.declarations.push_back(std::move(declaration));
    }

    return combined;
}

std::string shell_quote(const std::filesystem::path& path) {
    std::string quoted = "'";
    for (const char ch : path.string()) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

void print_diagnostic_with_source(std::string_view source, const std::string& message,
                                  const Lexer::SourceLocation& location) {
    std::cerr << message << '\n';

    if (location.offset >= source.size()) {
        return;
    }

    std::size_t line_begin = location.offset;
    while (line_begin > 0 && source[line_begin - 1] != '\n') {
        --line_begin;
    }

    std::size_t line_end = location.offset;
    while (line_end < source.size() && source[line_end] != '\n') {
        ++line_end;
    }

    const auto line = source.substr(line_begin, line_end - line_begin);
    std::cerr << line << '\n';

    std::size_t caret_column = location.column > 0 ? location.column - 1 : 0;
    for (std::size_t index = 0; index < caret_column; ++index) {
        if (index < line.size() && line[index] == '\t') {
            std::cerr << '\t';
        } else {
            std::cerr << ' ';
        }
    }
    std::cerr << "^\n";
}

void print_phase_timing(std::string_view phase_name,
                        std::chrono::steady_clock::duration duration) {
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    std::cerr << "[time] " << phase_name << ": " << micros << " us\n";
}

bool command_exists(std::string_view command_name) {
    const std::string command =
        "command -v " + std::string(command_name) + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

std::optional<std::string> find_x86_64_linker_command() {
    const char* override = std::getenv("MINIC_LINKER");
    if (override != nullptr && *override != '\0') {
        return std::string(override);
    }

#if defined(__linux__) && defined(__x86_64__)
    const char* compiler = std::getenv("CC");
    if (compiler != nullptr && *compiler != '\0') {
        return std::string(compiler);
    }
    return std::string("cc");
#elif defined(__linux__)
    if (command_exists("x86_64-linux-gnu-gcc")) {
        return std::string("x86_64-linux-gnu-gcc");
    }
    if (command_exists("x86_64-linux-gnu-cc")) {
        return std::string("x86_64-linux-gnu-cc");
    }
    return std::nullopt;
#else
    const char* compiler = std::getenv("CC");
    if (compiler != nullptr && *compiler != '\0') {
        return std::string(compiler);
    }
    return std::string("cc");
#endif
}

std::expected<void, std::string> assemble_and_link(const std::filesystem::path& assembly_path,
                                                   const std::filesystem::path& output_path) {
    auto linker_command = find_x86_64_linker_command();
    if (!linker_command) {
        return std::unexpected(
            "no x86-64 linker toolchain found; install 'x86_64-linux-gnu-gcc' or set MINIC_LINKER");
    }

    const std::filesystem::path runtime_path = MINIC_RUNTIME_PATH;
    const std::string command =
        *linker_command + " -no-pie " + shell_quote(assembly_path) + ' ' +
        shell_quote(runtime_path) + " -lm -o " + shell_quote(output_path);

    if (std::system(command.c_str()) != 0) {
        return std::unexpected("failed to assemble and link generated x86-64 program with runtime");
    }

    return {};
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: minic <source-file> [-o <output-file>] "
                     "[--dump-tokens|--dump-ast|--emit-asm|--time-phases]\n";
        return 1;
    }

    const std::filesystem::path source_path = argv[1];
    const std::string source_name = argv[1];
    bool dump_tokens = false;
    bool dump_ast = false;
    bool emit_asm = false;
    bool time_phases = false;
    std::filesystem::path output_path;

    for (int index = 2; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--dump-tokens") {
            dump_tokens = true;
        } else if (argument == "--dump-ast") {
            dump_ast = true;
        } else if (argument == "--emit-asm") {
            emit_asm = true;
        } else if (argument == "--time-phases") {
            time_phases = true;
        } else if (argument == "-o") {
            if (index + 1 >= argc) {
                std::cerr << "missing output file after -o\n";
                return 1;
            }
            output_path = argv[++index];
        } else {
            std::cerr << "unknown flag: " << argument << '\n';
            return 1;
        }
    }

    auto source_or_error = read_file(source_path);
    if (!source_or_error) {
        std::cerr << source_name << ": error: " << source_or_error.error() << '\n';
        return 1;
    }

    const auto lex_start = std::chrono::steady_clock::now();
    Lexer::Lexer lexer(*source_or_error, source_name);
    auto tokens_or_error = lexer.tokenize();
    const auto lex_end = std::chrono::steady_clock::now();
    if (!tokens_or_error) {
        print_diagnostic_with_source(*source_or_error,
                                     Lexer::format_error(tokens_or_error.error()),
                                     tokens_or_error.error().location);
        return 1;
    }
    if (time_phases) {
        print_phase_timing("lex", lex_end - lex_start);
    }

    if (dump_tokens) {
        print_tokens(*tokens_or_error);
        return 0;
    }

    const auto parse_start = std::chrono::steady_clock::now();
    Parser::Parser parser(std::move(*tokens_or_error), source_name);
    auto program_or_error = parser.parse_program();
    const auto parse_end = std::chrono::steady_clock::now();
    if (!program_or_error) {
        print_diagnostic_with_source(*source_or_error,
                                     Parser::format_error(program_or_error.error()),
                                     program_or_error.error().location);
        return 1;
    }
    if (time_phases) {
        print_phase_timing("parse", parse_end - parse_start);
    }

    if (dump_ast) {
        program_or_error->dump(std::cout);
        return 0;
    }

    auto combined_program_or_error = expand_program_imports(source_path, std::move(*program_or_error));
    if (!combined_program_or_error) {
        if (combined_program_or_error.error().location.has_value()) {
            print_diagnostic_with_source(
                combined_program_or_error.error().source, combined_program_or_error.error().message,
                *combined_program_or_error.error().location);
        } else {
            std::cerr << combined_program_or_error.error().message << '\n';
        }
        return 1;
    }

    const auto semantic_start = std::chrono::steady_clock::now();
    auto semantic_result = Semantic::analyze_program(*combined_program_or_error, source_name);
    const auto semantic_end = std::chrono::steady_clock::now();
    if (!semantic_result) {
        print_diagnostic_with_source(*source_or_error,
                                     Semantic::format_error(semantic_result.error()),
                                     semantic_result.error().location);
        return 1;
    }
    if (time_phases) {
        print_phase_timing("semantic", semantic_end - semantic_start);
    }

    const auto codegen_start = std::chrono::steady_clock::now();
    auto assembly_or_error =
        Codegen::generate_program(*combined_program_or_error, *semantic_result, source_name);
    const auto codegen_end = std::chrono::steady_clock::now();
    if (!assembly_or_error) {
        print_diagnostic_with_source(*source_or_error,
                                     Codegen::format_error(assembly_or_error.error()),
                                     assembly_or_error.error().location);
        return 1;
    }
    if (time_phases) {
        print_phase_timing("codegen", codegen_end - codegen_start);
    }

    if (output_path.empty()) {
#if defined(__linux__)
        if (!emit_asm && find_x86_64_linker_command().has_value()) {
            output_path = source_path;
            output_path.replace_extension();
        } else {
            output_path = source_path;
            output_path.replace_extension(".s");
        }
#else
        output_path = source_path;
        output_path.replace_extension(".s");
#endif
    }

    if (emit_asm || output_path.extension() == ".s") {
        if (!write_file(output_path, *assembly_or_error)) {
            std::cerr << output_path.string() << ": error: failed to write output file\n";
            return 1;
        }

        std::cout << "code generation successful: " << output_path.string() << '\n';
        return 0;
    }

    std::filesystem::path assembly_path = output_path;
    assembly_path += ".s";

    if (!write_file(assembly_path, *assembly_or_error)) {
        std::cerr << assembly_path.string() << ": error: failed to write output file\n";
        return 1;
    }

    const auto link_start = std::chrono::steady_clock::now();
    auto link_result = assemble_and_link(assembly_path, output_path);
    const auto link_end = std::chrono::steady_clock::now();
    if (!link_result) {
        std::cerr << output_path.string() << ": error: " << link_result.error() << '\n';
        std::cerr << "assembly was preserved at: " << assembly_path.string() << '\n';
        return 1;
    }
    if (time_phases) {
        print_phase_timing("link", link_end - link_start);
    }

    std::cout << "executable generation successful: " << output_path.string() << '\n';
    return 0;
}
