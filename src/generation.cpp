#include "generation.hpp"

#include "error.hpp"

auto Generator::generateProgram() -> std::string {
    _output << "global _start\n_start:\n";

    for (const auto &stmt: _program.stmts) {
        generateStmt(stmt);
    }
    _output << "    mov rax,60\n";
    _output << "    mov rdi,0\n";
    _output << "    syscall\n";
    return _output.str();
}
auto Generator::generateStmt(const node::Stmt &stmt) -> void {
    struct StmtVisitor {
        Generator& gen;

        auto operator()(const node::stmt::Exit &exit) const -> void {
            gen._output << ";; exit\n";
            gen.generateExpr(exit.expr);
            gen._output << "    mov rax,60\n";
            gen.pop("rdi");
            gen._output << "    syscall\n\n";
            gen._output << ";; /exit\n";
        }

        auto operator()(const node::stmt::Let &let) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == let.token.value.value();
                });
            if (it != gen._vars.cend()) {
                error("Variable already exists",let.token.location);
            }
            gen.generateExpr(let.expr);
            gen._vars.push_back({.name = let.token.value.value(), .stackLocation = gen._stackSize-1});
            gen._output << ";; let\n";
            gen._output << ";; /let\n";
        }

        auto operator()(const node::Scope &scope) const -> void {
            gen._output << ";; scope\n";
            gen.generateScope(scope);
            gen._output << ";; /scope\n";
        }
        auto operator()(const node::stmt::Assignment &assignment) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == assignment.token.value.value();
                });
            if (it == gen._vars.cend()) {
                error("Variable not found",assignment.token.location);
            }

            gen._output << ";; assignment\n";
            gen.generateExpr(assignment.expr);
            gen.pop("rax");
            gen._output << "    mov [rsp + " << (gen._stackSize - it->stackLocation-1)*8<< "],rax\n";
            gen._output << ";; /assignment\n";

        }

        auto operator()(const node::stmt::If &if_) const -> void {
            gen._output << ";; if\n";
            gen.generateExpr(if_.cond);
            gen.pop("rax");

            auto label = gen.makeLabel();
            gen._output << "    test rax, rax\n";
            gen._output << "    jz " << label << "\n";
            auto endLabel = gen.makeLabel();
            gen.generateScope(if_.scope);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (if_.next)
                gen.generateIfPred(*if_.next, endLabel);
            gen._output << endLabel << ":\n";
            gen._output << ";; /if\n";
        }
    };

    StmtVisitor visitor{.gen = *this};
    std::visit(visitor, stmt.var);
}

auto Generator::generateIfPred(const node::IfPred &ifPred, std::string& endLabel) -> void {
    struct PredVisitor {
        Generator& gen;
        std::string& endLabel;
        auto operator()(const node::pred::Elif &elif_) const -> void {

            gen._output << ";; elif\n";
            gen.generateExpr(elif_.cond);
            gen.pop("rax");

            const auto label = gen.makeLabel();
            gen._output << "    test rax, rax\n";
            gen._output << "    jz " << label << "\n";
            gen.generateScope(elif_.scope);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (elif_.next) gen.generateIfPred(*elif_.next,endLabel);
            gen._output << ";; /elif\n";
        }
        auto operator()(const node::pred::Else &else_) const -> void {
            gen._output << ";; else\n";
            gen.generateScope(else_.scope);
            gen._output << ";; /else\n";
        }
    };

    PredVisitor visitor{.gen = *this, .endLabel = endLabel};
    std::visit(visitor, ifPred.var);
}
auto Generator::generateTerm(const node::Term &term) -> void {
    struct TermVisitor {
        Generator& gen;

        auto operator()(const node::term::IntLit &intLit) const -> void {
            gen._output << ";; intLit\n";
            gen._output << "    mov rax, " << intLit.token.value.value() << "\n";
            gen.push("rax");
            gen._output << ";; /intLit\n";
        }

        auto operator()(const node::term::Ident &ident) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var)-> bool {
                    return var.name == ident.token.value.value();
                });
            if (it == gen._vars.cend()) {
                error("Variable not found",ident.token.location);
            }
            gen._output << ";; ident\n";
            std::stringstream offset;
            offset << "QWORD [rsp + " << (gen._stackSize - it->stackLocation - 1) * 8 << "]";
            gen.push(offset.str());
            gen._output << ";; /ident\n";
        }

        auto operator()(const node::term::Paren &paren) const -> void {
            gen._output << ";; paren\n";
            gen.generateExpr(*paren.expr);
            gen._output << ";; /paren\n";
        }
    };
    TermVisitor visitor{.gen = *this};
    std::visit(visitor, term.var);
}
auto Generator::generateExpr(const node::Expr &expr) -> void {
    struct ExprVisitor {
        Generator& gen;

        auto operator()(const node::Term &term) const -> void {
            gen.generateTerm(term);
        }
        auto operator()(const node::expr::Binary &bin) const -> void {
            gen.generateBinExpr(bin);
        }
    };
    ExprVisitor visitor({.gen = *this});
    std::visit(visitor, expr.var);
}
auto Generator::generateBinExpr(const node::expr::Binary &bin) -> void {
    struct BinExprVisitor {
        Generator &gen;

        auto operator()(const node::expr::Add &add) const -> void {
            gen.generateExpr(*add.rhs);
            gen.generateExpr(*add.lhs);
            gen.pop("rax");
            gen.pop("rbx");
            gen._output << "    add rax, rbx\n";
            gen.push("rax");
        }
        auto operator()(const node::expr::Sub &sub) const -> void {
            gen.generateExpr(*sub.rhs);
            gen.generateExpr(*sub.lhs);
            gen.pop("rax");
            gen.pop("rbx");
            gen._output << "    sub rax, rbx\n";
            gen.push("rax");
        }
        auto operator()(const node::expr::Mul &mul) const -> void {
            gen.generateExpr(*mul.rhs);
            gen.generateExpr(*mul.lhs);
            gen.pop("rax");
            gen.pop("rbx");
            gen._output << "    imul rax,rbx\n";
            gen.push("rax");
        }
        auto operator()(const node::expr::Div &div) const -> void {
            gen.generateExpr(*div.rhs);
            gen.generateExpr(*div.lhs);
            gen.pop("rax");
            gen.pop("rbx");
            gen._output << "    cqo\n";
            gen._output << "    idiv rbx\n";
            gen.push("rax");
        }
    };
    BinExprVisitor visitor{.gen = *this};
    std::visit(visitor, bin.var);
}
auto Generator::generateScope(const node::Scope &scope) -> void {
    beginScope();
    for (const auto &stmt: scope.stmts)
        generateStmt(stmt);
    endScope();
}


auto Generator::push(const std::string &reg) -> void {
    _output << "    push " << reg << "\n";
    _stackSize++;
}
auto Generator::pop(const std::string &reg) -> void {
    _output << "    pop " << reg << "\n";
    _stackSize--;
}
auto Generator::beginScope() -> void {
    _scopes.push_back(_vars.size());
}
auto Generator::endScope() -> void {
    const size_t popCount = _vars.size() - _scopes.back();
    _output << "    add rsp, " << popCount*8 << "\n";
    _stackSize -= popCount;
    for (size_t i{}; i < popCount; i++) _vars.pop_back();
    _scopes.pop_back();
}
auto Generator::makeLabel() -> std::string {
    return "label" + std::to_string(_labelCount++);
}


