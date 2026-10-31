#pragma once

#include "parser.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

// Still WIP
/** Assembly cheat sheet: x86-64, nasm (Intel) syntax, Linux. Only what this compiler emits or will soon.
 *
 *  Syntax
 *    op dst, src          the destination is ALWAYS first: `sub rax, rbx` means rax = rax - rbx
 *    [addr]               memory at that address: [rsp + 8] = the 8 bytes 8 above the stack top
 *    QWORD [addr]         says the memory operand is 8 bytes (needed when the size can't be inferred, e.g. push)
 *    name:                a label: a named position in the code, not an instruction (the CPU walks past it)
 *    ; text               a comment
 *    global _start        exports _start so the linker (ld) knows where the program begins
 *
 *  Registers (64-bit)
 *    rax                  main working register: expression results, syscall number
 *    rbx                  second operand of binary ops
 *    rdx                  high half for idiv (rdx:rax), remainder after idiv
 *    rdi                  first syscall argument (the exit code for exit)
 *    rsp                  stack pointer: address of the top of the stack. The stack grows DOWN (push lowers rsp)
 *    later: rsi, rcx, r8-r11 (more syscall/function args), rbp (frame pointer), rip (instruction pointer)
 *
 *  Data movement
 *    mov dst, src         dst = src (register, number, or memory; never memory to memory)
 *    push x               rsp -= 8, then [rsp] = x
 *    pop r                r = [rsp], then rsp += 8
 *    add rsp, n           drop n bytes off the stack without reading them (how endScope frees variables)
 *
 *  Arithmetic (signed, 64-bit)
 *    add a, b             a = a + b
 *    sub a, b             a = a - b
 *    imul a, b            a = a * b (signed; low 64 bits)
 *    cqo                  sign-extend rax into rdx:rax (fills rdx with rax's sign bit). Always right before idiv
 *    idiv r               signed divide rdx:rax by r -> quotient in rax, remainder in rdx. Divide by 0 crashes (SIGFPE)
 *    later: neg r (r = -r, for unary minus), inc / dec
 *
 *  Comparisons and jumps
 *    test a, a            sets flags from a AND a; ZF (zero flag) = 1 when a is 0. Used for "is the condition false?"
 *    jz label             jump if ZF = 1 (last result was zero)       later: jnz = jump if not zero
 *    jmp label            always jump
 *    later, for == != < > <= >= and loops:
 *    cmp a, b             computes a - b, sets flags, throws the result away
 *    sete/setne/setl/setg/setle/setge al
 *                         al = 1 if the last cmp was ==, !=, <, >, <=, >= (signed), else 0
 *    movzx rax, al        widen al (the low byte of rax) to all of rax, zero-filled
 *    je/jne/jl/jg/jle/jge label   jump directly on a cmp result
 *
 *  System calls (Linux)
 *    mov rax, <number>    which syscall: 60 = exit (later: 1 = write)
 *    mov rdi, <arg1>      first argument (exit code)
 *    syscall              ask the kernel to do it
 *
 *  Later, for functions
 *    call label           push the return address, jump to label
 *    ret                  pop the return address, jump back to it
 */

class Generator {
public:
    explicit Generator(node::Program program) : _program(std::move(program)) {
    }

    [[nodiscard]] auto generateProgram() -> std::string;

private:
    auto generateStmt(const node::Stmt &stmt) -> void;

    auto generateTerm(const node::Term &term) -> void;

    auto generateExpr(const node::Expr &expr) -> void;

    auto generateBinExpr(const node::expr::Binary &bin) -> void;

    auto generateScope(const node::Block &scope) -> void;

    auto generateIfPred(const node::IfPred &ifPred, std::string& endLabel) -> void;

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
