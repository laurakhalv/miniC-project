#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#ifndef MINIC_RUNTIME_PATH
#define MINIC_RUNTIME_PATH "runtime/minic_runtime.s"
#endif

namespace {

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
        shell_quote(runtime_path) + " -o " + shell_quote(output_path);

    if (std::system(command.c_str()) != 0) {
        return std::unexpected("failed to assemble and link generated x86-64 program with runtime");
    }

    return {};
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: minic <source-file> [-o <output-file>] [--dump-tokens|--dump-ast]\n";
        return 1;
    }

    const std::filesystem::path source_path = argv[1];
    const std::string source_name = argv[1];
    bool dump_tokens = false;
    bool dump_ast = false;
    std::filesystem::path output_path;

    for (int index = 2; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--dump-tokens") {
            dump_tokens = true;
        } else if (argument == "--dump-ast") {
            dump_ast = true;
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

    Lexer::Lexer lexer(*source_or_error, source_name);
    auto tokens_or_error = lexer.tokenize();
    if (!tokens_or_error) {
        std::cerr << Lexer::format_error(tokens_or_error.error()) << '\n';
        return 1;
    }

    if (dump_tokens) {
        print_tokens(*tokens_or_error);
        return 0;
    }

    Parser::Parser parser(std::move(*tokens_or_error), source_name);
    auto program_or_error = parser.parse_program();
    if (!program_or_error) {
        std::cerr << Parser::format_error(program_or_error.error()) << '\n';
        return 1;
    }

    if (dump_ast) {
        program_or_error->dump(std::cout);
        return 0;
    }

    auto semantic_result = Semantic::analyze_program(*program_or_error, source_name);
    if (!semantic_result) {
        std::cerr << Semantic::format_error(semantic_result.error()) << '\n';
        return 1;
    }

    auto assembly_or_error =
        Codegen::generate_program(*program_or_error, *semantic_result, source_name);
    if (!assembly_or_error) {
        std::cerr << Codegen::format_error(assembly_or_error.error()) << '\n';
        return 1;
    }

    if (output_path.empty()) {
#if defined(__linux__)
        if (find_x86_64_linker_command().has_value()) {
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

    if (output_path.extension() == ".s") {
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

    auto link_result = assemble_and_link(assembly_path, output_path);
    if (!link_result) {
        std::cerr << output_path.string() << ": error: " << link_result.error() << '\n';
        std::cerr << "assembly was preserved at: " << assembly_path.string() << '\n';
        return 1;
    }

    std::cout << "executable generation successful: " << output_path.string() << '\n';
    return 0;
}