#include "parser.hpp"

using Expr = node::Expr;
using Stmt = node::Stmt;




auto Parser::parseProgram() -> optional<node::Program> {
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
auto Parser::tryParseStmt() -> optional<Stmt> {
    if (!peek().has_value()) return nullopt;

    switch (peek().value().type) {
        case TokenType::let:
            return Stmt{.var = parseStmtLet()};
        case TokenType::exit:
            return Stmt{.var = parseStmtExit()};
        case TokenType::openCurly:
            return Stmt{.var = parseStmtScope()};
        case TokenType::branchIf:
            return Stmt{.var = parseStmtBranchIf()};
        default:
            return nullopt;
    }
}
auto Parser::parseStmtScope() -> node::Scope {
    // Statement: { let x = 5; }
    consume(); // |{| let x = 5; }
    node::Scope scope{}; //
    while (auto stmt = tryParseStmt()) // recursion to parse the entire scoped stmt { | let x = 5; | }
        scope.stmts.push_back(std::move(stmt.value())); //
    tryConsume(TokenType::closeCurly, "expected '}'"); // { let x = 5; |}|
    return scope;
}
auto Parser::parseStmtLet() -> node::stmt::Let {
    consume();
    auto token = tryConsume(TokenType::ident, "expected identifier");
    tryConsume(TokenType::eq, "expected '=' after identifier");
    auto expr = parseExpr();
    tryConsume(TokenType::semi, "expected ';'");
    return node::stmt::Let{.token = std::move(token), .expr = std::move(expr)};
}
auto Parser::parseStmtExit() -> node::stmt::Exit {
    // Statement: exit(x);
    consume(); // |exit|(x);
    tryConsume(TokenType::openParen, "expected '('"); // exit|(|x);
    auto expr = parseExpr(); // exit(|x|);
    tryConsume(TokenType::closeParen, "expected ')'"); // exit(x|)|;
    tryConsume(TokenType::semi, "expected ';'"); //exit(x)|;|
    return node::stmt::Exit{.expr = std::move(expr)};
}
auto Parser::parseStmtBranchIf() -> node::stmt::If {
    // if(x > 5) { do stuff; }
    consume(); // |if|(x > 5) { do stuff; }
    tryConsume(TokenType::openParen, "expected '('"); // if|(|x > 5) { do stuff; }
    auto expr = parseExpr(); // if(|x > 5|) { do stuff; }
    tryConsume(TokenType::closeParen, "expected ')'"); // if(x > 5|)| { do stuff; }
    auto scope = parseStmtScope();
    return node::stmt::If{.expr = std::move(expr), .scope = std::move(scope)};
}
auto Parser::parseExpr() -> Expr {
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
 * @return optional<Expr>
 */
auto Parser::tryParseExpr(int minPrecedence) -> optional<Expr> {
    auto term = tryParseTerm();
    if (!term) return nullopt;

    Expr lhs{.var = std::move(term.value())};

    while (true) {
        optional<Token> current = peek();
        optional<int> precedence = current ? binPrecedence(current.value().type) : nullopt;
        if (!precedence || *precedence < minPrecedence) break;

        const auto token = consume();
        auto rhs = tryParseExpr(precedence.value() + 1);
        if (!rhs.has_value()) {
            std::cerr << "Unable to parse expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        switch (token.type) {
            case TokenType::plus: {
                lhs = constructExpr<node::expr::Add>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            case TokenType::mul: {
                lhs = constructExpr<node::expr::Mul>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            case TokenType::sub: {
                lhs = constructExpr<node::expr::Sub>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            case TokenType::div: {
                lhs = constructExpr<node::expr::Div>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            default:
                break;
        }
    }
    return lhs;
}
auto Parser::tryParseTerm() -> optional<node::Term> {
    if (const auto &token = tryConsume(TokenType::intLit))
        return node::Term{.var = node::term::IntLit{.token = token.value()}};
    if (const auto &token = tryConsume(TokenType::ident))
        return node::Term{.var = node::term::Ident{.token = token.value()}};
    if (tryConsume(TokenType::openParen)) {
        auto expr = tryParseExpr();
        if (!expr.has_value()) {
            std::cerr << "Expected expression" << std::endl;
            exit(EXIT_FAILURE);
        }
        tryConsume(TokenType::closeParen, "Expected ')'");
        auto parenExpr = node::term::Paren{.expr = make_unique<Expr>(std::move(expr.value()))};
        return node::Term{.var = std::move(parenExpr)};
    }
    return nullopt;
}
auto Parser::peek(const size_t offset) const -> optional<Token> {
    if (_index + offset >= _tokens.size()) return std::nullopt;
    return _tokens[_index + offset];
}
auto Parser::tryConsume(const TokenType type) -> optional<Token> {
    if (peek().has_value() && peek().value().type == type)
        return consume();
    return nullopt;
}
auto Parser::tryConsume(const TokenType type, const std::string &errMsg) -> Token {
    if (peek().has_value() && peek().value().type == type)
        return consume();
    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);
}
auto Parser::consume() -> Token {
    return _tokens[_index++];
}
template<typename T>
auto Parser::constructExpr(Expr lhs, Expr rhs) -> Expr {
    T t{
        .lhs = make_unique<Expr>(std::move(lhs)),
        .rhs = make_unique<Expr>(std::move(rhs))
    };
    node::expr::Binary bin{.var = std::move(t)};
    return Expr{.var = std::move(bin)};
}




