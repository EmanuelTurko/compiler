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
    struct BinExprSub {
        unique_ptr<Expr> lhs;
        unique_ptr<Expr> rhs;
    };
    struct BinExpr {
        std::variant<BinExprAdd, BinExprMul,BinExprDiv,BinExprSub> var;
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
    struct StmtScope {
        std::vector<Stmt> stmts;
    };
    struct Stmt {
        std::variant<StmtExit, StmtLet,StmtScope> var;
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


    auto parseExpr() -> node::Expr;
    auto tryParseTerm() -> optional<node::Term>;
    auto tryParseExpr(int minPrecedence = 0) -> optional<node::Expr>;
    auto tryParseStmt() -> optional<node::Stmt>;
    auto parseProgram() -> optional<node::Program>;

private:
    [[nodiscard]] auto peek(size_t offset = 0) const -> optional<Token>;

    auto tryConsume(TokenType type) -> optional<Token>;
    auto tryConsume(TokenType type, const std::string &errMsg) -> Token;
    auto consume() -> Token;
    auto tryParseExpr(node::Expr expr) -> node::Expr;
    template<typename T>
    [[nodiscard]] auto constructExpr(node::Expr lhs,node::Expr rhs) -> node::Expr;

    const std::vector<Token> _tokens;
    size_t _index;
};

inline auto Parser::tryParseTerm() -> optional<node::Term> {
    if (const auto& IntLit = tryConsume(TokenType::intLit))
            return node::Term{.var = node::TermIntLit{.intLit = IntLit.value()}};
    if (const auto& Ident = tryConsume(TokenType::ident))
            return node::Term{.var = node::TermIdent{.ident = Ident.value()}};
    if (tryConsume(TokenType::openParen)) {

        auto expr = tryParseExpr();
        if (!expr.has_value()) {
            std::cerr << "Expected expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        tryConsume(TokenType::closeParen, "Expected ')'");
        auto parenExpr = node::TermParen{.parenExpr = make_unique<node::Expr>(std::move(expr.value()))};
        return node::Term{.var = std::move(parenExpr)};
    }
    return nullopt;
}

inline auto Parser::parseExpr() -> node::Expr {
    if (auto expr = tryParseExpr()) {
        return std::move(expr.value());
    }
    std::cerr << "Unable to parse expression" << std::endl;
    exit(EXIT_FAILURE);
}

/** 1. Read one term: every expression (Expr) starts with a term.
 *     If there is no term, there is no expression: return nullopt and let the caller decide if that's an error.
 *  2. Wrap the term in an Expr and call it left-hand side (lhs). lhs means "everything parsed so far".
 *     For x = 7; the 7 becomes lhs, nothing follows, so we just return lhs.
 *  3. Loop:
 *  3.1 Peek at the next token. If it's not an operator, or its precedence is below minPrecedence, stop looping.
 *      Stopping means "this operator belongs to a caller further up the recursion".
 *  3.2 Consume the operator (to step over it) and remember its type.
 *  3.3 Parse the right-hand side (rhs) with a recursive call, passing the consumed operator's precedence + 1.
 *      The +1 means the rhs only takes operators that bind tighter, so equal operators chain left-to-right.
 *  3.4 If there is no rhs, it's an error ("Expected expression").
 *  3.5 With lhs, rhs and op-type we construct the binary expression bottom-up (inside out):
 *      the op node (e.g. BinExprAdd) holds pointers to lhs and rhs, BinExpr holds the op node, Expr holds BinExpr.
 *  3.6 The constructed Expr replaces lhs, and the loop runs again.
 *  4. Return lhs.
 *
 *  Example: 1 + 2 * 3;   precedence: + = 0, * = 1
 *  outer  (min 0): term 1 -> lhs = 1. peek +, 0 >= 0, consume, parse rhs with min 1.
 *    inner  (min 1): term 2 -> lhs = 2. peek *, 1 >= 1, consume, parse rhs with min 2.
 *      innermost (min 2): term 3 -> lhs = 3. peek ;, not an operator, return 3.
 *    inner: lhs = (2 * 3). peek ;, stop, return (2 * 3).
 *  outer: lhs = 1 + (2 * 3). peek ;, stop, return.
 *
 *  Example: 1 + 2 + 3;
 *  outer (min 0): lhs = 1. peek +, consume, parse rhs with min 1.
 *    inner (min 1): lhs = 2. peek +, 0 < 1, stop, return 2.
 *  outer: lhs = (1 + 2). Loop again: peek +, 0 >= 0, consume, rhs = 3, lhs = ((1 + 2) + 3). peek ;, stop, return.
 *
 *  Example:5 + 3 * 2
 *  parseExpr(min 0)
    │ lhs = 5
    │ peek '+' (0 >= 0) → consume, rhs = parseExpr(min 1) ──┐
    │                                                        │
    │   parseExpr(min 1)                                     │
    │   │ lhs = 3                                            │
    │   │ peek '*' (1 >= 1) → consume, rhs = parseExpr(min 2)│
    │   │     parseExpr(min 2): lhs = 2, peek ';' → return 2 │
    │   │ lhs = (3 * 2)                                      │
    │   │ peek ';' → return (3 * 2) ─────────────────────────┘
    │ lhs = 5 + (3 * 2)
 * @param minPrecedence lowest operator precedence this call may consume
 * @return optional<node::Expr>
 */
inline auto Parser::tryParseExpr(int minPrecedence) -> optional<node::Expr> {
    auto term = tryParseTerm();
    if (!term) return nullopt;

    node::Expr lhs{.var = std::move(term.value())};

    while (true) {
        optional<Token> current = peek();
        optional<int> precedence = peek().has_value() ? binPrecedence(current.value().type) : nullopt;
        if (!precedence || precedence < minPrecedence) break;

        auto [type, value] = consume();
        auto rhs = tryParseExpr(precedence.value() + 1);
        if (!rhs.has_value()) {
            std::cerr << "Unable to parse expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        switch (type) {
            case TokenType::plus: {
                lhs = constructExpr<node::BinExprAdd>(std::move(lhs),std::move(rhs.value()));
                break;
            }
            case TokenType::mul: {
                lhs = constructExpr<node:: BinExprMul>(std::move(lhs),std::move(rhs.value()));
                break;
            }
            case TokenType::sub: {
                lhs = constructExpr<node::BinExprSub>(std::move(lhs),std::move(rhs.value()));
                break;
            }
            case TokenType::div: {
                lhs = constructExpr<node::BinExprDiv>(std::move(lhs),std::move(rhs.value()));
                break;
            }
            default:
                break;
        }
    }
    return lhs;
}
inline auto Parser::tryParseStmt() -> optional<node::Stmt> {
    if (!peek().has_value()) return nullopt;

    if (peek().value().type == TokenType::let) {
        consume();
        auto ident = tryConsume(TokenType::ident,"expected identifier");
        tryConsume(TokenType::eq,"expected '=' after identifier");
        auto expr = parseExpr();

        tryConsume(TokenType::semi,"expected ';'");
        return node::Stmt{.var = node::StmtLet{.ident = std::move(ident),.expr = std::move(expr)}};
    }
    if (peek().value().type == TokenType::exit) { // Statement: exit(x);
        consume(); // |exit|(x);
        tryConsume(TokenType::openParen,"expected '('"); // exit|(|x);
        auto expr= parseExpr(); // exit(|x|);
        tryConsume(TokenType::closeParen,"expected ')'"); // exit(x|)|;
        tryConsume(TokenType::semi,"expected ';'"); //exit(x)|;|
        return  node::Stmt{.var = node::StmtExit{.expr = std::move(expr)}};
    }
    if (peek().value().type == TokenType::openCurly) { // Statement: { let x = 5; }
        consume(); // |{| let x = 5; }
        node::StmtScope scope{};
        while (auto stmt = tryParseStmt()) // recursion to parse the entire scoped stmt { | let x = 5; | }
            scope.stmts.push_back(std::move(stmt.value()));
        tryConsume(TokenType::closeCurly,"expected '}'"); // { let x = 5; |}|
        return node::Stmt{.var = std::move(scope)};
    }
    return nullopt;
}
inline auto Parser::parseProgram() -> optional<node::Program> {
    node::Program program;
    while (peek().has_value()) {
        if (auto stmt = tryParseStmt()) {
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

inline auto Parser::tryConsume(const TokenType type,const std::string &errMsg) -> Token {
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

template<typename T>
auto Parser::constructExpr(node::Expr lhs, node::Expr rhs) -> node::Expr {
    T t{
        .lhs = make_unique<node::Expr>(std::move(lhs)),
        .rhs = make_unique<node::Expr>(std::move(rhs))
    };
    node::BinExpr bin{.var = std::move(t)};
    return node::Expr{.var = std::move(bin)};
}

