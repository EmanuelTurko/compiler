#pragma once

#include <variant>
#include <vector>
#include <memory>


#include "tokenization.hpp"

using std::optional;
using std::nullopt;
using std::unique_ptr;
using std::make_unique;


namespace node {
    struct Expr;

    struct TermIntLit {
        Token intLit;
    };

    struct TermIdent {
        Token ident;
    };

    struct BinExprAdd {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };

    struct BinExprMulti {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };

    struct BinExpr {
        std::variant<BinExprAdd, BinExprMulti> var;
    };

    struct Term {
        std::variant<TermIntLit, TermIdent> var;
    };
    struct Expr {
        std::variant<Term, BinExpr> var;
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
    explicit Parser(std::vector<Token> tokens, const size_t index = 0) :
    _tokens(std::move(tokens)),
    _index(index){}


    auto parseTerm() -> optional<node::Term>;
    auto parseBinExpr(node::Expr lhs) -> optional<node::BinExpr>;
    auto parseExpr() -> optional<node::Expr>;
    auto parseStmt() -> optional<node::Stmt>;
    auto parseProgram() -> optional<node::Program>;

private:
    [[nodiscard]] auto peek(size_t offset = 0) const -> optional<Token>;

    auto tryConsume(TokenType type) -> optional<Token>;
    auto tryConsume(TokenType type, const std::string &errMsg) -> optional<Token>;
    auto consume() -> Token;

    const std::vector<Token> _tokens;
    size_t _index;
};

inline auto Parser::parseTerm() -> optional<node::Term> {
    if (const auto& IntLit = tryConsume(TokenType::intLit))
            return node::Term{.var = node::TermIntLit{.intLit = IntLit.value()}};
    if (const auto& Ident = tryConsume(TokenType::ident))
            return node::Term{.var = node::TermIdent{.ident = Ident.value()}};
    return nullopt;
}

inline auto Parser::parseBinExpr(node::Expr lhs) -> optional<node::BinExpr> {
    consume(); // the '+', already checked by parseExpr
    auto lhsPtr = make_unique<node::Expr>(std::move(lhs));
    if (auto rhs = parseExpr()) {
        auto rhsPtr = make_unique<node::Expr>(std::move(rhs.value()));
        return node::BinExpr{.var = node::BinExprAdd{std::move(lhsPtr), std::move(rhsPtr)}};
    }
    std::cerr << "Expected expression" << std::endl;
    exit(EXIT_FAILURE);
}

inline auto Parser::parseExpr() -> optional<node::Expr> {
    auto term = parseTerm();
    if (!term) return nullopt;

    node::Expr lhs{.var = std::move(term.value())};
    if (tryConsume(TokenType::plus).has_value()) {
        if (auto binExpr = parseBinExpr(std::move(lhs)))
            return node::Expr{.var = std::move(binExpr.value())};
    }
    return lhs;
}
inline auto Parser::parseStmt() -> optional<node::Stmt> {
    if (peek().value().type == TokenType::exit && peek(1).has_value() && peek(1).value().type == TokenType::openParen) {
        consume();
        consume();
        node::StmtExit stmt;
        if (auto expr = parseExpr())
            stmt = {.expr = std::move(expr.value())};
        else {
            std::cerr << "Invalid expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        tryConsume(TokenType::closeParen,"Expected ')'");
        tryConsume(TokenType::semi,"Expected ';'");
        return node::Stmt{.var = std::move(stmt)};
    }
    if (
        peek().has_value() && peek().value().type == TokenType::let &&
        peek(1).has_value() && peek(1).value().type == TokenType::ident &&
        peek(2).has_value() && peek(2).value().type == TokenType::eq) {
        consume();
        auto stmt = node::StmtLet{.ident = consume(),.expr{}};
        consume();
        if (auto expr = parseExpr()) {
            stmt.expr = std::move(expr.value());
        } else {
            std::cerr << "Invalid expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        tryConsume(TokenType::semi,"Expected ';'");
        return node::Stmt{.var = std::move(stmt)};
        }
    return nullopt;
}
inline auto Parser::parseProgram() -> optional<node::Program> {
    node::Program program;
    while (peek().has_value()) {
        if (auto stmt = parseStmt()) {
            program.stmts.push_back(std::move(stmt.value()));
        } else {
            std::cerr << "Invalid statement1" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    return program;
}

inline auto Parser::tryConsume(const TokenType type) -> optional<Token> {
    if (peek().has_value() && peek().value().type == type)
        return consume();
    return nullopt;
}

inline auto Parser::tryConsume(const TokenType type,const std::string &errMsg) -> optional<Token> {
    if (peek().has_value() && peek().value().type == type)
        return consume();
    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);
}

inline auto Parser::consume() -> Token {
    return _tokens[_index++];
}

inline auto Parser::peek(const size_t offset) const -> optional<Token> {
    if (_index + offset >= _tokens.size()) return std::nullopt;
    return _tokens[_index + offset];
}

