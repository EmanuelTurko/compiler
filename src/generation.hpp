#pragma once

#include "parser.hpp"

#include <sstream>
#include <unordered_map>

class Generator {
public:
    explicit Generator(node::Program program):
        _program(std::move(program))
    {}

     auto generateExpr(const node::Expr& expr) -> void;
     auto generateStmt(const node::Stmt& stmt) -> void;
    [[nodiscard]] auto generateProgram() -> std::string;

private:
    auto push(const std::string& reg) -> void;
    auto pop(const std::string& reg) -> void;

    struct Var {
        size_t _stackLocation{};
    };
    const node::Program _program;
    std::stringstream _output{};
    size_t _stackSize{};
    std::unordered_map<std::string, Var> _vars{};
};

inline auto Generator::generateExpr(const node::Expr &expr) -> void {
    struct ExprVisitor {
        Generator* gen;

        auto operator()(const node::ExprIntLit& exprIntLit) const -> void {

            gen->_output << "    mov rax, " << exprIntLit.intLit.value.value() << "\n";
            gen->push("rax");
        }
        auto operator()(const node::ExprIdent& exprIdent) const -> void {

            if (const auto& ident = exprIdent.ident.value.value() ;!gen->_vars.contains(ident)) {
                std::cerr << "Undeclared identifier " << ident << std::endl;
                exit(EXIT_FAILURE);
            }
            const auto& idx = gen->_vars[exprIdent.ident.value.value()]._stackLocation;
            std::stringstream offset;
            offset << "QWORD [rsp + " << (gen->_stackSize - idx-1)*8 << "]";
            gen->push(offset.str());
        }
    };

    ExprVisitor visitor({.gen = this});
    std::visit(visitor,expr.var);
}

inline auto Generator::generateStmt(const node::Stmt& stmt) -> void {
    struct StmtVisitor {
        Generator* gen;
        auto operator()(const node::StmtExit& stmtExit)const ->void {

            gen->generateExpr(stmtExit.expr);
            gen->_output <<"    mov rax,60\n";
            gen->pop("rdi");
            gen->_output <<"    syscall\n\n";
        }
        auto operator()(const node::StmtLet& stmtLet)const ->void {
            if (const auto& ident = stmtLet.ident.value.value(); gen->_vars.contains(ident)){
                std::cerr << "Identifier already exists" << ident << std::endl;
                exit(EXIT_FAILURE);
            }
            gen->_vars.insert({stmtLet.ident.value.value(),Var{._stackLocation = gen->_stackSize}});
            gen->generateExpr(stmtLet.expr);
        }
    };

    StmtVisitor visitor{.gen = this};
    std::visit(visitor,stmt.var);
}

inline auto Generator::generateProgram() -> std::string {
    _output << "global _start\n_start:\n";

    for (const auto& stmt : _program.stmts) {
        generateStmt(stmt);
    }
    _output <<"    mov rax,60\n";
    _output <<"    mov rdi,0\n";
    _output <<"    syscall\n\n";
    return _output.str();
}

inline auto Generator::push(const std::string &reg) -> void {
    _output << "    push " << reg << "\n";
    _stackSize++;
}

inline auto Generator::pop(const std::string &reg) -> void {
    _output << "    pop " << reg << "\n";
    _stackSize--;
}
