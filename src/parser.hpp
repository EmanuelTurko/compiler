#pragma once

#include <cassert>
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
    struct Stmt;

    struct TermIntLit {
        Token intLit;
    };
    struct TermIdent {
        Token ident;
    };
    struct TermParen {
        unique_ptr<Expr> parenExpr;
    };
    struct BinExprAdd {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };
    struct BinExprMul {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };
    struct BinExprDiv {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };
    struct BinExprMinus {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };
    struct BinExpr {
        std::variant<BinExprAdd, BinExprMul,BinExprDiv,BinExprMinus> var;
    };
    struct Term {
        std::variant<TermIntLit, TermIdent,TermParen> var;
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

    struct Scope {
        std::vector<Stmt> stmts;
    };
    struct StmtIf {
        Expr expr;
        Scope scope;
    };
    struct Stmt {
        std::variant<StmtExit, StmtLet,Scope,StmtIf> var;
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

    auto parseProgram() -> optional<node::Program>;

private:

    auto tryParseStmt() -> optional<node::Stmt>;
    auto parseStmtScope() -> node::Scope;
    auto parseStmtLet() -> node::StmtLet;
    auto parseStmtExit() -> node::StmtExit;
    auto parseStmtBranchIf() -> node::StmtIf;
    auto parseExpr() -> node::Expr;
    auto tryParseExpr(int minPrecedence = 0) -> optional<node::Expr>;
    auto tryParseTerm() -> optional<node::Term>;

    [[nodiscard]] auto peek(size_t offset = 0) const -> optional<Token>;

    auto tryConsume(TokenType type) -> optional<Token>;
    auto tryConsume(TokenType type, const std::string &errMsg) -> Token;
    auto consume() -> Token;



    template<typename T>
    [[nodiscard]] auto constructExpr(node::Expr lhs,node::Expr rhs) -> node::Expr;

    const std::vector<Token> _tokens;
    size_t _index;
};
