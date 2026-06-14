#include "ast.hpp"

#include <ostream>
#include <utility>

namespace {

//helpers для единообразного форматирования дерева

//печатает отступ
void write_indent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) {
        out << "  ";
    }
}

//печатает одну строку дерева с отступом и переносом
void write_line(std::ostream& out, int indent, const std::string& text) {
    write_indent(out, indent);
    out << text << '\n';
}

//склеивает части составного имени в одну строку ::
std::string join_name(const std::vector<std::string>& parts) {
    std::string result;

    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result += "::";
        }
        result += parts[i];
    }

    return result;
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

}

namespace AST {

Node::Node(SourceRange source_range) : range(source_range) {}

namespace {

std::string visibility_to_string(Visibility visibility) {
    return visibility == Visibility::Public ? "public" : "private";
}

std::string export_to_string(bool is_exported) {
    return is_exported ? "export" : "private";
}


}  

//выводит один параметр функции
//default_value выводится только если есть (доп A.2.10)
void Parameter::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "Parameter " + name);
    type->dump(out, indent + 1);
    if (default_value) {
        write_line(out, indent + 1, "Default");
        default_value->dump(out, indent + 2);
    }
}

//выводит одно поле структуры
void FieldDecl::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "Field " + visibility_to_string(visibility) + " " + name);
    type->dump(out, indent + 1);
}

//выводит инициализацию одного поля при создании структуры
void FieldInitializer::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "FieldInit " + name);
    value->dump(out, indent + 1);
}

void ImportSpec::dump(std::ostream& out, int indent) const {
    std::string line = "Import " + join_module_name(module_path);
    if (imported_path.has_value()) {
        line += "::" + join_name(*imported_path);
    }
    write_line(out, indent, line);
}

//корень дерева выводит всю программу
void Program::dump(std::ostream& out) const {
    out << "Program\n";
    if (module_name.has_value()) {
        write_line(out, 1, "Module " + join_module_name(*module_name));
    }
    for (const auto& import : imports) {
        import.dump(out, 1);
    }
    // на верхнем уровне просто печатаем все объявления по порядку
    for (const auto& declaration : declarations) {
        declaration->dump(out, 1);
    }
}

//сохраняет части имени типа и размер массива
TypeSyntax::TypeSyntax(SourceRange range, std::vector<std::string> parts,
                       std::optional<std::string> size)
    : Node(range), name_parts(std::move(parts)), array_size(std::move(size)) {}

    //выводит тип (int32-Type int32)
void TypeSyntax::dump(std::ostream& out, int indent) const {
    std::string line = "Type " + join_name(name_parts);
    if (array_size.has_value()) {
        line += "[" + *array_size + "]";
    }
    write_line(out, indent, line);
}
//принимает все данные функции и сохраняет их
FunctionDecl::FunctionDecl(SourceRange range, std::string function_name,
                           std::vector<Parameter> params,
                           std::unique_ptr<TypeSyntax> result_type,
                           std::unique_ptr<BlockStmt> function_body, bool method,
                           Visibility member_visibility)
    : Decl(range),
      name(std::move(function_name)),
      parameters(std::move(params)),
      return_type(std::move(result_type)),
      body(std::move(function_body)),
      is_method(method),
      visibility(member_visibility) {}

//выводит объявление функции 
void FunctionDecl::dump(std::ostream& out, int indent) const {
    const std::string prefix = is_method ? "MethodDecl " : "FunctionDecl ";
    if (is_method) {
        write_line(out, indent, prefix + visibility_to_string(visibility) + " " + name);
    } else if (has_module_visibility) {
        write_line(out, indent, prefix + export_to_string(is_exported) + " " + name);
    } else {
        write_line(out, indent, prefix + name);
    }
    // Параметры и return type выделяются отдельно, чтобы AST было легче читать глазами.
    if (!parameters.empty()) {
        write_line(out, indent + 1, "Parameters");
        for (const auto& parameter : parameters) {
            parameter.dump(out, indent + 2);
        }
    }
    write_line(out, indent + 1, "ReturnType");
    return_type->dump(out, indent + 2);
    body->dump(out, indent + 1);
}

//принимает имя, поля и методы
StructDecl::StructDecl(SourceRange range, std::string struct_name, std::vector<FieldDecl> field_list,
                       std::vector<std::unique_ptr<FunctionDecl>> method_list)
    : Decl(range),
      name(std::move(struct_name)),
      fields(std::move(field_list)),
      methods(std::move(method_list)) {}

      //выводит структуру имя, потом все поля, потом все методы
void StructDecl::dump(std::ostream& out, int indent) const {
    const std::string line = has_module_visibility
                                 ? "StructDecl " + export_to_string(is_exported) + " " + name
                                 : "StructDecl " + name;
    write_line(out, indent, line);
    for (const auto& field : fields) {
        field.dump(out, indent + 1);
    }
    for (const auto& method : methods) {
        method->dump(out, indent + 1);
    }
}

//конструктор узла type alias
TypeAliasDecl::TypeAliasDecl(SourceRange range, std::string alias_name,
                             std::unique_ptr<TypeSyntax> target_type)
    : Decl(range), name(std::move(alias_name)), aliased_type(std::move(target_type)) {}

//функция печати type alias в AST
void TypeAliasDecl::dump(std::ostream& out, int indent) const {
    const std::string line = has_module_visibility
                                 ? "TypeAliasDecl " + export_to_string(is_exported) + " " + name
                                 : "TypeAliasDecl " + name;
    write_line(out, indent, line);
    aliased_type->dump(out, indent + 1);
}


NamespaceDecl::NamespaceDecl(SourceRange range, std::string namespace_name,
                             std::vector<std::unique_ptr<Decl>> nested_declarations)
    : Decl(range), name(std::move(namespace_name)), declarations(std::move(nested_declarations)) {}

void NamespaceDecl::dump(std::ostream& out, int indent) const {
    const std::string line = has_module_visibility
                                 ? "NamespaceDecl " + export_to_string(is_exported) + " " + name
                                 : "NamespaceDecl " + name;
    write_line(out, indent, line);
    for (const auto& declaration : declarations) {
        declaration->dump(out, indent + 1);
    }
}

BlockStmt::BlockStmt(SourceRange range, std::vector<std::unique_ptr<Stmt>> body_statements)
    : Stmt(range), statements(std::move(body_statements)) {}

void BlockStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "BlockStmt");
    // блок список вложенных инструкций
    for (const auto& statement : statements) {
        statement->dump(out, indent + 1);
    }
}

//все инструкции(stmt) устроены одинаково конструктор сохраняет данные, 
//dump() выводит имя узла и рекурсивно дочерние узлы

VariableDeclStmt::VariableDeclStmt(SourceRange range, Mutability variable_mutability,
                                   std::unique_ptr<TypeSyntax> variable_type,
                                   std::string variable_name,
                                   std::unique_ptr<Expr> init_expr)
    : Stmt(range),
      mutability(variable_mutability),
      type(std::move(variable_type)),
      name(std::move(variable_name)),
      initializer(std::move(init_expr)) {}

void VariableDeclStmt::dump(std::ostream& out, int indent) const {
    const std::string qualifier = mutability == Mutability::Mutable ? "let" : "const";
    write_line(out, indent, "VariableDeclStmt " + qualifier + " " + name);
    type->dump(out, indent + 1);
    initializer->dump(out, indent + 1);
}

IfStmt::IfStmt(SourceRange range, std::unique_ptr<Expr> if_condition,
               std::unique_ptr<BlockStmt> then_block,
               std::unique_ptr<BlockStmt> else_block)
    : Stmt(range),
      condition(std::move(if_condition)),
      then_branch(std::move(then_block)),
      else_branch(std::move(else_block)) {}

void IfStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IfStmt");
    write_line(out, indent + 1, "Condition");
    condition->dump(out, indent + 2);
    write_line(out, indent + 1, "Then");
    then_branch->dump(out, indent + 2);
    if (else_branch) {
        write_line(out, indent + 1, "Else");
        else_branch->dump(out, indent + 2);
    }
}

WhileStmt::WhileStmt(SourceRange range, std::unique_ptr<Expr> while_condition,
                     std::unique_ptr<BlockStmt> loop_body)
    : Stmt(range), condition(std::move(while_condition)), body(std::move(loop_body)) {}

void WhileStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "WhileStmt");
    write_line(out, indent + 1, "Condition");
    condition->dump(out, indent + 2);
    write_line(out, indent + 1, "Body");
    body->dump(out, indent + 2);
}

ReturnStmt::ReturnStmt(SourceRange range, std::unique_ptr<Expr> return_value)
    : Stmt(range), value(std::move(return_value)) {}

void ReturnStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "ReturnStmt");
    if (value) {
        value->dump(out, indent + 1);
    }
}

BreakStmt::BreakStmt(SourceRange range) : Stmt(range) {}

void BreakStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "BreakStmt");
}

ContinueStmt::ContinueStmt(SourceRange range) : Stmt(range) {}

void ContinueStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "ContinueStmt");
}

ExprStmt::ExprStmt(SourceRange range, std::unique_ptr<Expr> expr)
    : Stmt(range), expression(std::move(expr)) {}

void ExprStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "ExprStmt");
    expression->dump(out, indent + 1);
}

EmptyStmt::EmptyStmt(SourceRange range) : Stmt(range) {}

void EmptyStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "EmptyStmt");
}

//присваивание. oтдельно хранится цель присваивания и выражение-значение
AssignmentExpr::AssignmentExpr(SourceRange range, std::unique_ptr<Expr> assignment_target,
                               std::unique_ptr<Expr> assignment_value)
    : Expr(range), target(std::move(assignment_target)), value(std::move(assignment_value)) {}

void AssignmentExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "AssignmentExpr");
    write_line(out, indent + 1, "Target");
    target->dump(out, indent + 2);
    write_line(out, indent + 1, "Value");
    value->dump(out, indent + 2);
}

//бинарная операция 
BinaryExpr::BinaryExpr(SourceRange range, TokenType type, std::string lexeme,
                       std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
    : Expr(range),
      op_type(type),
      op_lexeme(std::move(lexeme)),
      left(std::move(lhs)),
      right(std::move(rhs)) {}

void BinaryExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "BinaryExpr " + op_lexeme);
    left->dump(out, indent + 1);
    right->dump(out, indent + 1);
}

//унарная
UnaryExpr::UnaryExpr(SourceRange range, TokenType type, std::string lexeme,
                     std::unique_ptr<Expr> expr)
    : Expr(range), op_type(type), op_lexeme(std::move(lexeme)), operand(std::move(expr)) {}

void UnaryExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "UnaryExpr " + op_lexeme);
    operand->dump(out, indent + 1);
}

//явное привидение типа
CastExpr::CastExpr(SourceRange range, std::unique_ptr<TypeSyntax> cast_type,
                   std::unique_ptr<Expr> cast_expression)
    : Expr(range), target_type(std::move(cast_type)), expression(std::move(cast_expression)) {}

void CastExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "CastExpr");
    write_line(out, indent + 1, "TargetType");
    target_type->dump(out, indent + 2);
    write_line(out, indent + 1, "Expression");
    expression->dump(out, indent + 2);
}

//вызов функции
CallExpr::CallExpr(SourceRange range, std::unique_ptr<Expr> target,
                   std::vector<CallArgument> args)
    : Expr(range), callee(std::move(target)), arguments(std::move(args)) {}

void CallArgument::dump(std::ostream& out, int indent) const {
    if (name.has_value()) {
        write_line(out, indent, "NamedArgument " + *name);
    } else {
        write_line(out, indent, "Argument");
    }
    value->dump(out, indent + 1);
}
//поддержкf именованных аргументов(часть допа A.2.10)
void CallExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "CallExpr");
    write_line(out, indent + 1, "Callee");
    callee->dump(out, indent + 2);
    // aргументы печатаются после callee в том порядке, в котором были в исходнике
    for (const auto& argument : arguments) {
        argument.dump(out, indent + 1);
    }
}

//доступ по индексу
IndexExpr::IndexExpr(SourceRange range, std::unique_ptr<Expr> indexed_base,
                     std::unique_ptr<Expr> index_expr)
    : Expr(range), base(std::move(indexed_base)), index(std::move(index_expr)) {}

void IndexExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IndexExpr");
    base->dump(out, indent + 1);
    index->dump(out, indent + 1);
}

//доступ к полю структуры через точку
FieldAccessExpr::FieldAccessExpr(SourceRange range, std::unique_ptr<Expr> field_base,
                                 std::string field_name)
    : Expr(range), base(std::move(field_base)), field(std::move(field_name)) {}

void FieldAccessExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "FieldAccessExpr ." + field);
    base->dump(out, indent + 1);
}

IdentifierExpr::IdentifierExpr(SourceRange range, std::string identifier_name)
    : Expr(range), name(std::move(identifier_name)) {}

void IdentifierExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IdentifierExpr " + name);
}

//составное имя через ::
NamespaceAccessExpr::NamespaceAccessExpr(SourceRange range, std::vector<std::string> qualified_name)
    : Expr(range), path(std::move(qualified_name)) {}

void NamespaceAccessExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "NamespaceAccessExpr " + join_name(path));
}


//дальше литеральные expression-узлы
IntLiteralExpr::IntLiteralExpr(SourceRange range, std::string literal_value)
    : Expr(range), value(std::move(literal_value)) {}

void IntLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IntLiteralExpr " + value);
}

FloatLiteralExpr::FloatLiteralExpr(SourceRange range, std::string literal_value)
    : Expr(range), value(std::move(literal_value)) {}

void FloatLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "FloatLiteralExpr " + value);
}

StringLiteralExpr::StringLiteralExpr(SourceRange range, std::string literal_value)
    : Expr(range), value(std::move(literal_value)) {}

void StringLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "StringLiteralExpr " + value);
}

CharLiteralExpr::CharLiteralExpr(SourceRange range, std::string literal_value)
    : Expr(range), value(std::move(literal_value)) {}

void CharLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "CharLiteralExpr " + value);
}

BoolLiteralExpr::BoolLiteralExpr(SourceRange range, bool literal_value)
    : Expr(range), value(literal_value) {}

void BoolLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, std::string("BoolLiteralExpr ") + (value ? "true" : "false"));
}

//доп A.2.1: if как выражение
IfExpr::IfExpr(SourceRange range, std::unique_ptr<Expr> if_condition,
               std::unique_ptr<Expr> then_expr, std::unique_ptr<Expr> else_expr)
    : Expr(range),
      condition(std::move(if_condition)),
      then_branch(std::move(then_expr)),
      else_branch(std::move(else_expr)) {}

void IfExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IfExpr");
    write_line(out, indent + 1, "Condition");
    condition->dump(out, indent + 2);
    write_line(out, indent + 1, "Then");
    then_branch->dump(out, indent + 2);
    write_line(out, indent + 1, "Else");
    else_branch->dump(out, indent + 2);
}

//создания структуры через литерал
StructLiteralExpr::StructLiteralExpr(SourceRange range, std::vector<std::string> type_name,
                                     std::vector<FieldInitializer> field_initializers)
    : Expr(range), type_path(std::move(type_name)), fields(std::move(field_initializers)) {}

void StructLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "StructLiteralExpr " + join_name(type_path));
    for (const auto& field : fields) {
        field.dump(out, indent + 1);
    }
}

//узел для литерала массива
ArrayLiteralExpr::ArrayLiteralExpr(SourceRange range,
                                   std::vector<std::unique_ptr<Expr>> literal_elements)
    : Expr(range), elements(std::move(literal_elements)) {}

void ArrayLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "ArrayLiteralExpr");
    for (const auto& element : elements) {
        element->dump(out, indent + 1);
    }
}

} 
