#include "generation.hpp"

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
            gen.generateExpr(exit.expr);
            gen._output << "    mov rax,60\n";
            gen.pop("rdi");
            gen._output << "    syscall\n\n";
        }

        auto operator()(const node::stmt::Let &let) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == let.token.value.value();
                });
            if (it != gen._vars.cend()) {
                std::cerr << "Variable " << let.token.value.value() << " already exists" << std::endl;
                exit(EXIT_FAILURE);
            }
            gen._vars.push_back({.name = let.token.value.value(), .stackLocation = gen._stackSize});
            gen.generateExpr(let.expr);
        }

        auto operator()(const node::Scope &scope) const -> void {
            gen.generateScope(scope);
        }
        auto operator()(const node::stmt::Assignment &assignment) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == assignment.token.value.value();
                });
            if (it == gen._vars.cend()) {
                std::cerr << "Variable " << assignment.token.value.value() << " not found" << std::endl;
                exit(EXIT_FAILURE);
            }

            gen.generateExpr(assignment.expr);
            gen.pop("rax");
            gen._output << "    mov [rsp + " << gen._stackSize - it->stackLocation << "],rax\n";

        }
        auto operator()(const node::stmt::If &_if) const -> void {
            gen.generateExpr(_if.cond);
            gen.pop("rax");

            auto label = gen.makeLabel();
            gen._output << "    test rax, rax\n";
            gen._output << "    jz " << label << "\n";
            gen.generateScope(_if.scope);
            gen._output << label << ":\n";
            if (_if.next) gen.generateIfPred(*_if.next,label);
        }
    };

    StmtVisitor visitor{.gen = *this};
    std::visit(visitor, stmt.var);
}

auto Generator::generateIfPred(const node::IfPred &ifPred, std::string& endLabel) -> void {
    struct PredVisitor {
        Generator& gen;
        std::string& endLabel;
        auto operator()(const node::pred::Elif &predElif) const -> void {

            gen.generateExpr(predElif.cond);
            gen.pop("rax");

            const auto label = gen.makeLabel();
            gen._output << "    test rax, rax\n";
            gen._output << "    jz " << label << "\n";
            gen.generateScope(predElif.scope);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (predElif.next) gen.generateIfPred(*predElif.next,endLabel);
        }
        auto operator()(const node::pred::Else &predElse) const -> void {
            gen.generateScope(predElse.scope);
        }
    };

    PredVisitor visitor{.gen = *this, .endLabel = endLabel};
    std::visit(visitor, ifPred.var);
}
auto Generator::generateTerm(const node::Term &term) -> void {
    struct TermVisitor {
        Generator& gen;

        auto operator()(const node::term::IntLit &intLit) const -> void {
            gen._output << "    mov rax, " << intLit.token.value.value() << "\n";
            gen.push("rax");
        }

        auto operator()(const node::term::Ident &ident) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var)-> bool {
                    return var.name == ident.token.value.value();
                });
            if (it == gen._vars.cend()) {
                std::cerr << "Variable " << ident.token.value.value() << " not found" << std::endl;
                exit(EXIT_FAILURE);
            }

            std::stringstream offset;
            offset << "QWORD [rsp + " << (gen._stackSize - it->stackLocation - 1) * 8 << "]";
            gen.push(offset.str());
        }

        auto operator()(const node::term::Paren &paren) const -> void {
            gen.generateExpr(*paren.expr);
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
            gen._output << "    mul rbx\n";
            gen.push("rax");
        }
        auto operator()(const node::expr::Div &div) const -> void {
            gen.generateExpr(*div.rhs);
            gen.generateExpr(*div.lhs);
            gen.pop("rax");
            gen.pop("rbx");
            gen._output << "    div rbx\n";
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
    _output << "    add rsp, " << popCount << "\n";
    _stackSize -= popCount;
    for (size_t i{}; i < popCount; i++) _vars.pop_back();
    _scopes.pop_back();
}
auto Generator::makeLabel() -> std::string {
    return "label" + std::to_string(_labelCount++);
}


