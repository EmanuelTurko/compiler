#pragma once

#include <algorithm>

#include "parser.hpp"

#include <sstream>
#include <map>
#include <cassert>
#include <utility>

class Generator {
public:
    explicit Generator(node::Program program) : _program(std::move(program)) {
    }

    auto generateBinExpr(const node::BinExpr &binExpr);

    auto generateTerm(const node::Term &term) -> void;

    auto generateExpr(const node::Expr &expr) -> void;

    auto generateStmt(const node::Stmt &stmt) -> void;

    auto generateScope(const node::Scope &scope) -> void;

    [[nodiscard]] auto generateProgram() -> std::string;

private:
    auto push(const std::string &reg) -> void;

    auto pop(const std::string &reg) -> void;

    auto beginScope() -> void;
    auto endScope() -> void;
    auto makeLabel() -> std::string;

    struct Var {
        std::string name{};
        size_t stackLocation{};
    };

    const node::Program _program;
    std::stringstream _output{};
    size_t _stackSize{};
    size_t _labelCount{};
    std::vector<Var> _vars{};
    std::vector<size_t> _scopes{};
};

inline auto Generator::generateBinExpr(const node::BinExpr &binExpr) {
    struct BinExprVisitor {
        Generator *gen;

        auto operator()(const node::BinExprAdd &binExprAdd) const -> void {
            gen->generateExpr(*binExprAdd.rhs);
            gen->generateExpr(*binExprAdd.lhs);
            gen->pop("rax");
            gen->pop("rbx");
            gen->_output << "    add rax, rbx\n";
            gen->push("rax");
        }

        auto operator()(const node::BinExprSub &binExprSub) const -> void {
            gen->generateExpr(*binExprSub.rhs);
            gen->generateExpr(*binExprSub.lhs);
            gen->pop("rax");
            gen->pop("rbx");
            gen->_output << "    sub rax, rbx\n";
            gen->push("rax");
        }

        auto operator()(const node::BinExprMul &binExprMul) const -> void {
            gen->generateExpr(*binExprMul.rhs);
            gen->generateExpr(*binExprMul.lhs);
            gen->pop("rax");
            gen->pop("rbx");
            gen->_output << "    mul rbx\n";
            gen->push("rax");
        }

        auto operator()(const node::BinExprDiv &binExprDiv) const -> void {
            gen->generateExpr(*binExprDiv.rhs);
            gen->generateExpr(*binExprDiv.lhs);
            gen->pop("rax");
            gen->pop("rbx");
            gen->_output << "    div rbx\n";
            gen->push("rax");
        }
    };
    BinExprVisitor visitor{.gen = this};
    std::visit(visitor, binExpr.var);
}

inline auto Generator::generateTerm(const node::Term &term) -> void {
    struct TermVisitor {
        Generator *gen;

        auto operator()(const node::TermIntLit &termIntLit) const -> void {
            gen->_output << "    mov rax, " << termIntLit.intLit.value.value() << "\n";
            gen->push("rax");
        }

        auto operator()(const node::TermIdent &termIdent) const -> void {
            /*if (const auto &ident = termIdent.ident.value.value(); !gen->_vars.contains(ident)) {
                std::cerr << "Undeclared identifier " << ident << std::endl;
                exit(EXIT_FAILURE);
            }*/

            const auto it = std::ranges::find_if(
                std::as_const(gen->_vars),[&](const Var& var)->bool {
                return var.name == termIdent.ident.value.value();
            });
            if (it == gen->_vars.cend()) {
                std::cerr << "Variable " << termIdent.ident.value.value() << " not found" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::stringstream offset;
            offset << "QWORD [rsp + " << (gen->_stackSize - it->stackLocation - 1) * 8 << "]";
            gen->push(offset.str());
        }
        auto operator()(const node::TermParen &termParen) const -> void {
            gen->generateExpr(*termParen.parenExpr);
        }
    };
    TermVisitor visitor{.gen = this};
    std::visit(visitor, term.var);
}

inline auto Generator::generateExpr(const node::Expr &expr) -> void {
    struct ExprVisitor {
        Generator *gen;

        auto operator()(const node::Term &term) const -> void {
            gen->generateTerm(term);
        }
        auto operator()(const node::BinExpr &binExpr) const -> void {
            gen->generateBinExpr(binExpr);
        }

    };

    ExprVisitor visitor({.gen = this});
    std::visit(visitor, expr.var);
}

inline auto Generator::generateStmt(const node::Stmt &stmt) -> void {
    struct StmtVisitor {
        Generator *gen;

        auto operator()(const node::StmtExit &stmtExit) const -> void {
            gen->generateExpr(stmtExit.expr);
            gen->_output << "    mov rax,60\n";
            gen->pop("rdi");
            gen->_output << "    syscall\n\n";
        }

        auto operator()(const node::StmtLet &stmtLet) const -> void {
            /*if (const auto &ident = stmtLet.ident.value.value(); gen->_vars.contains(ident)) {
                std::cerr << "Identifier already exists" << ident << std::endl;
                exit(EXIT_FAILURE);
            }*/

            const auto it = std::ranges::find_if(
                std::as_const(gen->_vars),[&](const Var& var) -> bool {
                   return var.name == stmtLet.ident.value.value();
                });
            if (it != gen->_vars.cend()) {
                std::cerr << "Variable " << stmtLet.ident.value.value() << " already exists" << std::endl;
                exit(EXIT_FAILURE);
            }
            gen->_vars.push_back({.name = stmtLet.ident.value.value(), .stackLocation = gen->_stackSize});
            gen->generateExpr(stmtLet.expr);
        }
        auto operator()(const node::Scope &stmtScope) const -> void {
           gen->generateScope(stmtScope);
        }
        auto operator()(const node::StmtIf &stmtIf) const -> void {
           gen->generateExpr(stmtIf.expr);
            gen->pop("rax");

            const auto label = gen->makeLabel();
            gen->_output << "    test rax, rax\n";
            gen->_output << "    jz " << label << "\n";
            gen->generateScope(stmtIf.scope);
            gen->_output << label << ":\n";
        }
    };

    StmtVisitor visitor{.gen = this};
    std::visit(visitor, stmt.var);
}

inline auto Generator::generateProgram() -> std::string {
    _output << "global _start\n_start:\n";

    for (const auto &stmt: _program.stmts) {
        generateStmt(stmt);
    }
    _output << "    mov rax,60\n";
    _output << "    mov rdi,0\n";
    _output << "    syscall\n";
    return _output.str();
}

inline auto Generator::generateScope(const node::Scope &scope) -> void {
    beginScope();
    for (const auto& stmt : scope.stmts)
        generateStmt(stmt);
    endScope();
}

inline auto Generator::push(const std::string &reg) -> void {
    _output << "    push " << reg << "\n";
    _stackSize++;
}

inline auto Generator::pop(const std::string &reg) -> void {
    _output << "    pop " << reg << "\n";
    _stackSize--;
}

inline auto Generator::beginScope() -> void {
    _scopes.push_back(_vars.size());
}
inline auto Generator::endScope() -> void {
    const size_t popCount = _vars.size() - _scopes.back();
    _output << "    add rsp, " << popCount << "\n";
    _stackSize-= popCount;

    for (size_t i{}; i<popCount; i++) _vars.pop_back();
    _scopes.pop_back();

}

inline auto Generator::makeLabel() -> std::string {
    return "label" + std::to_string(_labelCount++);
}
