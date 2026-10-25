#pragma once

#include <variant>

#include "tokenization.hpp"

#include <vector>

using std::optional;
using std::nullopt;

namespace node {
    struct ExprIntLit {
        Token intLit;
    };

    struct ExprIdent {
        Token ident;
    };

    struct Expr {
        std::variant<ExprIntLit, ExprIdent> var;
    };

    struct StmtExit {
        Expr expr;
    };

    struct StmtLet {
        Token ident;
        Expr expr;
    };

    struct Stmt {
        std::variant<StmtExit, StmtLet> var;
    };

    struct Program {
        std::vector<Stmt> stmts;
    };
}

class Parser {
public:
    explicit Parser(std::vector<Token> tokens, const size_t index = 0) : _tokens(std::move(tokens)),
                                                                         _index(index) {
    }


    auto parseProgram() -> optional<node::Program>;

    auto parseExpr() -> optional<node::Expr>;

    auto parseStmt() -> optional<node::Stmt>;

private:
    [[nodiscard]] auto peek(size_t offset = 0) const -> optional<Token>;

    auto consume() -> Token;

    const std::vector<Token> _tokens;
    size_t _index;
};

inline auto Parser::parseProgram() -> optional<node::Program> {
    node::Program program;
    while (peek().has_value()) {
        if (auto stmt = parseStmt()) {
            program.stmts.push_back(stmt.value());
        } else {
            std::cerr << "Invalid statement1" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    return program;
}

inline auto Parser::parseExpr() -> optional<node::Expr> {
    if (peek().has_value() && peek().value().type == TokenType::intLit)
        return node::Expr{.var = node::ExprIntLit{.intLit = consume()}};
    if (peek().has_value() && peek().value().type == TokenType::ident)
        return node::Expr{.var = node::ExprIdent{.ident = consume()}};
    return nullopt;
}

inline auto Parser::parseStmt() -> optional<node::Stmt> {
    if (peek().value().type == TokenType::exit && peek(1).has_value() && peek(1).value().type == TokenType::openParen) {
        consume();
        consume();
        node::StmtExit stmt;
        if (auto expr = parseExpr())
            stmt = {.expr = expr.value()};
        else {
            std::cerr << "Invalid expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        if (peek().has_value() && peek().value().type == TokenType::closeParen)
            consume();
        else {
            std::cerr << "Expected ')'" << std::endl;
        }
        if (peek().has_value() && peek().value().type == TokenType::semi)
            consume();
        else {
            std::cerr << "Expected ';'" << std::endl;
            exit(EXIT_FAILURE);
        }
        return node::Stmt{.var = stmt};
    }
    if (
        peek().has_value() && peek().value().type == TokenType::let &&
        peek(1).has_value() && peek(1).value().type == TokenType::ident &&
        peek(2).has_value() && peek(2).value().type == TokenType::eq) {
        consume();
        auto stmt = node::StmtLet{.ident = consume()};
        consume();
        if (auto expr = parseExpr()) {
            stmt.expr = expr.value();
        } else {
            std::cerr << "Invalid expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        if (peek().has_value() && peek().value().type == TokenType::semi) {
            consume();
        } else {
            std::cerr << "Expected ';'" << std::endl;
            exit(EXIT_FAILURE);
        }
        return node::Stmt{.var = stmt};
    }
    return nullopt;
}

inline auto Parser::consume() -> Token {
    return _tokens[_index++];
}

inline auto Parser::peek(const size_t offset) const -> optional<Token> {
    if (_index + offset >= _tokens.size()) return std::nullopt;
    return _tokens[_index + offset];
}
