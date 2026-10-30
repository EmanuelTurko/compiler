#pragma once

#include "parser.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

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

    auto generateScope(const node::Scope &scope) -> void;

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
