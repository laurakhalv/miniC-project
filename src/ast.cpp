#include "ast.hpp"

#include <ostream>
#include <utility>

namespace {

void write_indent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) {
        out << "  ";
    }
}

void write_line(std::ostream& out, int indent, const std::string& text) {
    write_indent(out, indent);
    out << text << '\n';
}

std::string join_name(const std::vector<std::string>& parts) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "::";
        result += parts[i];
    }
    return result;
}

} 

namespace AST {

Node::Node(SourceRange source_range) : range(source_range) {}



void Parameter::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "Parameter " + name);
    type->dump(out, indent + 1);
}

void FieldDecl::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "Field " + name);
    type->dump(out, indent + 1);
}

void FieldInitializer::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "FieldInit " + name);
    value->dump(out, indent + 1);
}



void Program::dump(std::ostream& out) const {
    out << "Program\n";
    for (const auto& decl : declarations) {
        decl->dump(out, 1);
    }
}



TypeSyntax::TypeSyntax(SourceRange range, std::vector<std::string> parts,
                       std::optional<std::string> size)
    : Node(range), name_parts(std::move(parts)), array_size(std::move(size)) {}

void TypeSyntax::dump(std::ostream& out, int indent) const {
    std::string line = "Type " + join_name(name_parts);
    if (array_size.has_value()) {
        line += "[" + *array_size + "]";
    }
    write_line(out, indent, line);
}


FunctionDecl::FunctionDecl(SourceRange range, std::string function_name,
                           std::vector<Parameter> params,
                           std::unique_ptr<TypeSyntax> result_type,
                           std::unique_ptr<BlockStmt> function_body)
    : Decl(range),
      name(std::move(function_name)),
      parameters(std::move(params)),
      return_type(std::move(result_type)),
      body(std::move(function_body)) {}

void FunctionDecl::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "FunctionDecl " + name);
    if (!parameters.empty()) {
        write_line(out, indent + 1, "Parameters");
        for (const auto& param : parameters) {
            param.dump(out, indent + 2);
        }
    }
    write_line(out, indent + 1, "ReturnType");
    return_type->dump(out, indent + 2);
    body->dump(out, indent + 1);
}

StructDecl::StructDecl(SourceRange range, std::string struct_name,
                       std::vector<FieldDecl> field_list)
    : Decl(range), name(std::move(struct_name)), fields(std::move(field_list)) {}

void StructDecl::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "StructDecl " + name);
    for (const auto& field : fields) {
        field.dump(out, indent + 1);
    }
}

TypeAliasDecl::TypeAliasDecl(SourceRange range, std::string alias_name,
                             std::unique_ptr<TypeSyntax> target_type)
    : Decl(range), name(std::move(alias_name)), aliased_type(std::move(target_type)) {}

void TypeAliasDecl::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "TypeAliasDecl " + name);
    aliased_type->dump(out, indent + 1);
}

NamespaceDecl::NamespaceDecl(SourceRange range, std::string namespace_name,
                             std::vector<std::unique_ptr<Decl>> nested_declarations)
    : Decl(range),
      name(std::move(namespace_name)),
      declarations(std::move(nested_declarations)) {}

void NamespaceDecl::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "NamespaceDecl " + name);
    for (const auto& decl : declarations) {
        decl->dump(out, indent + 1);
    }
}



BlockStmt::BlockStmt(SourceRange range,
                     std::vector<std::unique_ptr<Stmt>> body_statements)
    : Stmt(range), statements(std::move(body_statements)) {}

void BlockStmt::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "BlockStmt");
    for (const auto& stmt : statements) {
        stmt->dump(out, indent + 1);
    }
}

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
    const std::string qualifier =
        mutability == Mutability::Mutable ? "let" : "const";
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
    : Stmt(range),
      condition(std::move(while_condition)),
      body(std::move(loop_body)) {}

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



AssignmentExpr::AssignmentExpr(SourceRange range,
                               std::unique_ptr<Expr> assignment_target,
                               std::unique_ptr<Expr> assignment_value)
    : Expr(range),
      target(std::move(assignment_target)),
      value(std::move(assignment_value)) {}

void AssignmentExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "AssignmentExpr");
    write_line(out, indent + 1, "Target");
    target->dump(out, indent + 2);
    write_line(out, indent + 1, "Value");
    value->dump(out, indent + 2);
}

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

UnaryExpr::UnaryExpr(SourceRange range, TokenType type, std::string lexeme,
                     std::unique_ptr<Expr> expr)
    : Expr(range),
      op_type(type),
      op_lexeme(std::move(lexeme)),
      operand(std::move(expr)) {}

void UnaryExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "UnaryExpr " + op_lexeme);
    operand->dump(out, indent + 1);
}

CastExpr::CastExpr(SourceRange range, std::unique_ptr<TypeSyntax> cast_type,
                   std::unique_ptr<Expr> cast_expression)
    : Expr(range),
      target_type(std::move(cast_type)),
      expression(std::move(cast_expression)) {}

void CastExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "CastExpr");
    write_line(out, indent + 1, "TargetType");
    target_type->dump(out, indent + 2);
    write_line(out, indent + 1, "Expression");
    expression->dump(out, indent + 2);
}

CallExpr::CallExpr(SourceRange range, std::unique_ptr<Expr> target,
                   std::vector<std::unique_ptr<Expr>> args)
    : Expr(range), callee(std::move(target)), arguments(std::move(args)) {}

void CallExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "CallExpr");
    write_line(out, indent + 1, "Callee");
    callee->dump(out, indent + 2);
    if (!arguments.empty()) {
        write_line(out, indent + 1, "Arguments");
        for (const auto& arg : arguments) {
            arg->dump(out, indent + 2);
        }
    }
}

IndexExpr::IndexExpr(SourceRange range, std::unique_ptr<Expr> indexed_base,
                     std::unique_ptr<Expr> index_expr)
    : Expr(range),
      base(std::move(indexed_base)),
      index(std::move(index_expr)) {}

void IndexExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IndexExpr");
    base->dump(out, indent + 1);
    index->dump(out, indent + 1);
}

FieldAccessExpr::FieldAccessExpr(SourceRange range,
                                 std::unique_ptr<Expr> field_base,
                                 std::string field_name)
    : Expr(range),
      base(std::move(field_base)),
      field(std::move(field_name)) {}

void FieldAccessExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "FieldAccessExpr ." + field);
    base->dump(out, indent + 1);
}

IdentifierExpr::IdentifierExpr(SourceRange range, std::string identifier_name)
    : Expr(range), name(std::move(identifier_name)) {}

void IdentifierExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "IdentifierExpr " + name);
}

NamespaceAccessExpr::NamespaceAccessExpr(SourceRange range,
                                         std::vector<std::string> qualified_name)
    : Expr(range), path(std::move(qualified_name)) {}

void NamespaceAccessExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "NamespaceAccessExpr " + join_name(path));
}

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

BoolLiteralExpr::BoolLiteralExpr(SourceRange range, bool literal_value)
    : Expr(range), value(literal_value) {}

void BoolLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent,
               std::string("BoolLiteralExpr ") + (value ? "true" : "false"));
}

StructLiteralExpr::StructLiteralExpr(SourceRange range,
                                     std::vector<std::string> type_name,
                                     std::vector<FieldInitializer> field_initializers)
    : Expr(range),
      type_path(std::move(type_name)),
      fields(std::move(field_initializers)) {}

void StructLiteralExpr::dump(std::ostream& out, int indent) const {
    write_line(out, indent, "StructLiteralExpr " + join_name(type_path));
    for (const auto& field : fields) {
        field.dump(out, indent + 1);
    }
}

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
