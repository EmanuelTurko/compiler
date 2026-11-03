#include "generation.hpp"

#include <cassert>

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
    const auto it =
        std::ranges::find_if(_program.functions,[](const node::Fun &fun) -> bool {
            return fun.name.token.value.value() == "main";
    });
    if (it == _program.functions.cend()) throw std::runtime_error("main() function not found");

    _output << "global _start\n\n";
    _output << "_start:\n";
    _output << "    call main\n";
    mov("rdi","rax");
    mov("rax","60");
    _output << "    syscall\n\n";

    for (const auto &fun: _program.functions) {
        generateFun(fun);
    }
    return _output.str();
}

auto Generator::generateFun(const node::Fun& fun) -> void {

    _vars.clear(); _stackSize = 0;

    _output << fun.name.token.value.value() << ":\n";
    push("rbp");
    mov("rbp","rsp");
    for (size_t i = 0; i < fun.params.size();++i) {
        push(paramRegisters[i]);
        _vars.push_back(Var{
            .name = fun.params[i].ident.token.value.value(),
            .stackLocation = _stackSize-1
        });
    }
    generateBlock(fun.block);
    mov("rsp","rbp");
    pop("rbp");
    _output << "    ret\n\n";
}


auto Generator::generateStmt(const node::Stmt &stmt) -> void {
    struct StmtVisitor {
        Generator& gen;

        auto operator()(const node::stmt::Ret &ret) const -> void {
                if (ret.expr.has_value()) gen.generateExpr(ret.expr.value());
                else gen.mov("rax","0");
                gen.pop("rax");
                gen.mov("rsp","rbp");
                gen._output << "    pop rbp\n";
                gen._output << "    ret\n";
            }

        auto operator()(const node::stmt::Def &def) const -> void {
            const auto val = def.ident.token.value.value();
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == val;
                });
            if (it != gen._vars.cend()) {
                error("Variable already exists",def.ident.token.location);
            }
            gen.generateExpr(def.expr.value());
            gen._vars.push_back({.name = val, .stackLocation = gen._stackSize-1});
        }

        auto operator()(const node::Block &scope) const -> void {
            gen.generateBlock(scope);
        }
        auto operator()(const node::stmt::Assign &assign) const -> void {
            const auto it = std::ranges::find_if(
                std::as_const(gen._vars), [&](const Var &var) -> bool {
                    return var.name == assign.ident.token.value.value();
                });
            if (it == gen._vars.cend()) {
                error("Variable not found",assign.ident.token.location);
            }

            gen.generateExpr(assign.expr);
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
            gen.generateBlock(if_.block);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (if_.next)
                gen.generateIfPred(*if_.next, endLabel);
            gen._output << endLabel << ":\n";
        }
        auto operator()(const node::stmt::While &while_) const ->void{
            assert("while not impleneted");
        }
        auto operator()(const node::stmt::ExprStmt& exprStmt) const -> void {
            assert("exprstmt not impleneted");
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
            gen.generateBlock(elif_.block);
            gen._output << "    jmp " << endLabel << "\n";
            gen._output << label << ":\n";
            if (elif_.next) gen.generateIfPred(*elif_.next,endLabel);
        }
        auto operator()(const node::pred::Else &else_) const -> void {
            gen.generateBlock(else_.block);
        }
    };

    PredVisitor visitor{.gen = *this, .endLabel = endLabel};
    std::visit(visitor, ifPred.var);
}
/*
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
}*/
auto Generator::generateExpr(const node::Expr &expr) -> void {
    struct ExprVisitor {
        Generator& gen;

        auto operator()(const node::Literal& literal) const -> void {
            assert(false &&"literal not impleneted");
        }
        auto operator()(const node::expr::Unary& unary) const -> void {
            assert(false &&"unary not impleneted");
        }
        auto operator()(const node::expr::Binary &bin) const -> void {
            gen.generateBinExpr(bin);
        }
        auto operator()(const node::expr::Ident& ident) const -> void {
            const auto it = std::ranges::find_if(gen._vars,[&](const Var& var)-> bool {
                return var.name == ident.token.value.value();
            });
            if (it == gen._vars.cend())
                error("Identifier not found",ident.token.location);

            std::stringstream ss{};
            ss << "QWORD [rsp + " << (gen._stackSize - it->stackLocation-1)*8<< "]";
            gen.push(ss.str());
        }
        auto operator()(const node::expr::Call& call) const -> void {
            //

        }

    };
    ExprVisitor visitor({.gen = *this});
    std::visit(visitor, expr.var);
}
auto Generator::generateBinExpr(const node::expr::Binary &bin) -> void {

     generateExpr(*bin.lhs);
    generateExpr(*bin.rhs);
    pop("rax");
    pop("rbx");
    _output << "    add rax, rbx\n";
    push("rax");
}
auto Generator::generateBlock(const node::Block &block) -> void {
    beginBlock();
    for (const auto &stmt: block.stmts)
        generateStmt(stmt);
    endBlock();
}


auto Generator::push(const std::string &reg) -> void {
    _output << "    push " << reg << "\n";
    _stackSize++;
}
auto Generator::pop(const std::string &reg) -> void {
    _output << "    pop " << reg << "\n";
    _stackSize--;
}



auto Generator::mov(const std::string &reg1, const std::string &reg2) -> void {
    _output << "    mov " << reg1 << "," << reg2 << "\n";
}

auto Generator::beginBlock() -> void {
    _blocks.push_back(_vars.size());
}
auto Generator::endBlock() -> void {
    const size_t popCount = _vars.size() - _blocks.back();
    _output << "\n    add rsp, " << popCount*8 << "\n";
    _stackSize -= popCount;
    for (size_t i{}; i < popCount; i++) _vars.pop_back();
    _blocks.pop_back();
}
auto Generator::makeLabel() -> std::string {
    return "label" + std::to_string(_labelCount++);
}