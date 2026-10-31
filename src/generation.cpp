#include "generation.hpp"

#include "error.hpp"

/** Walks the AST and writes x86-64 assembly (nasm syntax) into _output. One pass, top to bottom, no optimisation.
 *  main.cpp writes the result to out.asm, then nasm + ld turn it into the executable `out`.
 *  (Instructions used are explained in generation.hpp.)
 *
 *  The model: everything lives on the stack.
 *  1. Every expression leaves exactly ONE value on top of the stack (push). Whoever needs the value pops it.
 *     push()/pop() also keep _stackSize (counted in 8-byte slots, not bytes) in sync with the real stack.
 *  2. A variable IS a stack slot. `let` evaluates its expression and leaves the result where it is;
 *     that slot becomes the variable. _vars remembers {name, stackLocation}, where stackLocation is the
 *     slot number counted from the bottom (first push = slot 0). The variable is registered only AFTER its
 *     expression is generated, so `let x = x;` is "Variable not found".
 *  3. To find a variable, convert "slot from the bottom" into "bytes from the top" (rsp points at the top):
 *         offset = (_stackSize - stackLocation - 1) * 8      ->  [rsp + offset]
 *     Reading pushes a copy of that slot; assignment evaluates the new value, pops it into rax, and writes it there.
 *  4. Binary ops: generate rhs, then lhs (so lhs ends on top), pop rax = lhs, pop rbx = rhs,
 *     operate into rax, push rax. Math is signed (imul, cqo + idiv).
 *  5. Scopes: beginScope() remembers how many vars exist; endScope() frees everything declared since
 *     (add rsp, count * 8), forgets those names, and lowers _stackSize by count.
 *  6. if / elif / else: each condition is evaluated and popped into rax; `test rax, rax` + `jz` skips the body
 *     when it's 0 (0 = false, anything else = true).
 *     Each link has its own "false" label, placed right after its body = the start of the next link.
 *     The whole chain shares one end label, placed after the last link; every taken body jumps there.
 *  7. exit(expr): evaluate, pop into rdi, syscall 60. The program always ends with an implicit exit(0).
 *
 *  Example: let x = 7; let y = x - 2; exit(y);
 *
 *  code                          asm                             stack (top on the right)   _stackSize
 *  let x = 7;                    mov rax, 7 / push rax           [x=7]                       1   x -> slot 0
 *  let y = x - 2;  rhs 2         mov rax, 2 / push rax           [7, 2]                      2
 *                  lhs x         push QWORD [rsp + 8]            [7, 2, 7]                   3   (3-0-1)*8 = 8
 *                  op            pop rax / pop rbx               [7]                         1
 *                                sub rax, rbx / push rax         [7, y=5]                    2   y -> slot 1
 *  exit(y);                      push QWORD [rsp + 0]            [7, 5, 5]                   3   (3-1-1)*8 = 0
 *                                mov rax, 60 / pop rdi / syscall                                 exit code 5
 *
 *  Example: if(c){ A } elif(d){ B } else { C }
 *          <c>  pop rax / test rax, rax / jz label0   ; c false -> try the elif
 *          A                                         ; if body
 *          jmp label1                                ; taken -> skip the rest of the chain
 *  label0: <d>  pop rax / test rax, rax / jz label2   ; d false -> go to the else
 *          B
 *          jmp label1
 *  label2: C                                         ; last link: falls into label1 by itself
 *  label1:                                           ; end of the whole chain
 */
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

        auto operator()(const node::stmt::Ret &exit) const -> void {
            gen.generateExpr(exit.expr);
            gen._output << "    mov rax,60\n";
            gen.pop("rdi");
            gen._output << "    syscall\n\n";
        }

        auto operator()(const node::stmt::Def &let) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == let.token.value.value();
                });
            if (it != gen._vars.cend()) {
                error("Variable already exists",let.token.location);
            }
            gen.generateExpr(let.expr);
            gen._vars.push_back({.name = let.token.value.value(), .stackLocation = gen._stackSize-1});
        }

        auto operator()(const node::Block &scope) const -> void {
            gen.generateScope(scope);
        }
        auto operator()(const node::stmt::Assignment &assignment) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == assignment.token.value.value();
                });
            if (it == gen._vars.cend()) {
                error("Variable not found",assignment.token.location);
            }

            gen.generateExpr(assignment.expr);
            gen.pop("rax");
            gen._output << "    mov [rsp + " << (gen._stackSize - it->stackLocation-1)*8<< "],rax\n";

        }

        auto operator()(const node::stmt::If &if_) const -> void {
            gen.generateExpr(if_.cond);
            gen.pop("rax");

            auto label = gen.makeLabel();
            gen._output << "    test rax, rax\n";
            gen._output << "    jz " << label << "\n";
            auto endLabel = gen.makeLabel();
            gen.generateScope(if_.block);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (if_.next)
                gen.generateIfPred(*if_.next, endLabel);
            gen._output << endLabel << ":\n";
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

            gen.generateExpr(elif_.cond);
            gen.pop("rax");

            const auto label = gen.makeLabel();
            gen._output << "    test rax, rax\n";
            gen._output << "    jz " << label << "\n";
            gen.generateScope(elif_.scope);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (elif_.next) gen.generateIfPred(*elif_.next,endLabel);
        }
        auto operator()(const node::pred::Else &else_) const -> void {
            gen.generateScope(else_.scope);
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
                error("Variable not found",ident.token.location);
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
auto Generator::generateScope(const node::Block &scope) -> void {
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


