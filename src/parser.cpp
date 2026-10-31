#include "parser.hpp"

#include "error.hpp"

using Expr = node::Expr;
using Stmt = node::Stmt;

/** Parser: turns the flat token list into a tree (the AST) that the generator walks.
 *  Recursive descent: one function per grammar rule (docs/grammer.md), each one consumes exactly the tokens of its rule.
 *
 *  Naming convention:
 *    tryParseX()  - "maybe there's an X here": returns nullopt/nullptr WITHOUT error if X doesn't start here.
 *    parseX()     - "there MUST be an X here": errors and exits if it isn't.
 *    tryConsume(type)        - consume the token if it has that type, otherwise nullopt.
 *    tryConsume(type, msg)   - consume the token if it has that type, otherwise error(msg).
 *  Errors use getLocation(): the current token's line:col, or the last token's at end of file.
 *
 *  Grammar rule        -> function             -> node
 *    Prog              -> parseProgram         -> node::Program   { vector<Stmt> }
 *    Stmt              -> tryParseStmt         -> node::Stmt      variant<Exit, Let, Scope, If, Assignment>
 *      exit(Expr);     -> parseStmtExit        -> stmt::Exit
 *      let ident=Expr; -> parseStmtLet         -> stmt::Let
 *      ident = Expr;   -> parseAssignment      -> stmt::Assignment
 *      if(Expr) Scope  -> parseStmtBranchIf    -> stmt::If        { cond, scope, next }
 *      { Stmt* }       -> parseScope           -> node::Scope     { vector<Stmt> }
 *    IfPred            -> tryParseIfPred       -> node::IfPred    variant<Elif, Else>, or nullptr if neither follows
 *    Expr              -> parseExpr / tryParseExpr (precedence climbing, see below)
 *                                              -> node::Expr      variant<Term, Binary>
 *    Term              -> tryParseTerm         -> node::Term      variant<IntLit, Ident, Paren>
 *
 *  Which statement is it? tryParseStmt looks at ONE token (LL(1)): let / exit / { / ident / if.
 *
 *  The if chain is a linked list: If.next -> Elif.next -> Elif.next -> Else (Else has no next).
 *  Children are held by unique_ptr where a node contains its own type (Expr inside Expr, IfPred inside IfPred),
 *  because a struct can't hold itself by value.
 *
 *  Example: let x = 1 + 2; if(x){ exit(x); } else { exit(0); }
 *  Program
 *  |- Let  x
 *  |  `- Binary Add
 *  |     |- Term IntLit 1
 *  |     `- Term IntLit 2
 *  `- If
 *     |- cond:  Term Ident x
 *     |- scope: { Exit(Term Ident x) }
 *     `- next:  Else
 *               `- scope: { Exit(Term IntLit 0) }
 */
auto Parser::parseProgram() -> optional<node::Program> {
    node::Program program;
    while (peek().has_value()) {
        if (auto stmt = tryParseStmt()) {
            program.stmts.push_back(std::move(stmt.value()));
        } else {
            error("Invalid statement",getLocation());
        }
    }
    return program;
}
auto Parser::tryParseStmt() -> optional<Stmt> {
    if (!peek().has_value()) return nullopt;

    switch (peek().value().type) {
        case TokenType::kwDef:
            return Stmt{.var = parseStmtLet()};
        case TokenType::kwRet:
            return Stmt{.var = parseStmtRet()};
        case TokenType::openCurly:
            return Stmt{.var = parseScope()};
        case TokenType::ident:
            return Stmt{.var = parseAssignment()};
        case TokenType::kwIf:
            return Stmt{.var = parseStmtBranchIf()};
        default:
            return nullopt;
    }
}
auto Parser::parseScope() -> node::Block {
    // Statement: { let x = 5; }
    consume(); // |{| let x = 5; }
    node::Block scope{}; //
    while (auto stmt = tryParseStmt()) // recursion to parse the entire scoped stmt { | let x = 5; | }
        scope.stmts.push_back(std::move(stmt.value())); //
    tryConsume(TokenType::closeCurly, "expected '}'"); // { let x = 5; |}|
    return scope;
}
auto Parser::parseStmtLet() -> node::stmt::Def {
    consume();
    auto token = tryConsume(TokenType::ident, "expected identifier");
    tryConsume(TokenType::assign, "expected '=' after identifier");
    auto expr = parseExpr();
    tryConsume(TokenType::semi, "expected ';'");
    return node::stmt::Def{.token = std::move(token), .expr = std::move(expr)};
}
auto Parser::parseStmtRet() -> node::stmt::Ret {
    // Statement: exit(x);
    consume(); // |exit|(x);
    auto expr = parseExpr(); // exit(|x|);
    tryConsume(TokenType::semi, "expected ';'"); //exit(x)|;|
    return node::stmt::Ret{.expr = std::move(expr)};
}
auto Parser::parseStmtBranchIf() -> node::stmt::If {
    // if(x > 5) { do stuff; }
    consume();
    auto cond = parseCondition();
    auto scope = parseScope();
    auto next = tryParseIfPred();
    return node::stmt::If{
        .cond = std::move(cond),
        .block = std::move(scope),
        .next = std::move(next)
    };
}
auto Parser::parseCondition() -> node::Expr {
    tryConsume(TokenType::openParen, "Expected '('");
    auto cond = parseExpr();
    tryConsume(TokenType::closeParen, "Expected ')'");

    return cond;
}

auto Parser::parseAssignment() -> node::stmt::Assignment {
    const auto token = consume(); // get the target
    tryConsume(TokenType::assign, "Expected '='");
    auto expr = parseExpr();
    tryConsume(TokenType::semi, "Expected ';'");
    return {.token = token,.expr = std::move(expr)};
}

auto Parser::parseExpr() -> Expr {
    if (auto expr = tryParseExpr()) {
        return std::move(expr.value());
    }
    error("Unable to parse expression",getLocation());
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
 *  3.4 If there is no rhs, it's an error ("Unable to parse expression").
 *  3.5 With lhs, rhs and op-type we construct the binary expression bottom-up (inside out):
 *      the op node (e.g. expr::Add) holds pointers to lhs and rhs, expr::Binary holds the op node, Expr holds Binary.
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
            error("Unable to parse expression",getLocation());
        }
        switch (token.type) {
            case TokenType::plus: {
                lhs = constructExpr<node::expr::Add>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            case TokenType::star: {
                lhs = constructExpr<node::expr::Mul>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            case TokenType::minus: {
                lhs = constructExpr<node::expr::Sub>(std::move(lhs), std::move(rhs.value()));
                break;
            }
            case TokenType::slash: {
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
    if (const auto &token = tryConsume(TokenType::litInt))
        return node::Term{.var = node::term::IntLit{.token = token.value()}};
    if (const auto &token = tryConsume(TokenType::ident))
        return node::Term{.var = node::term::Ident{.token = token.value()}};
    if (tryConsume(TokenType::openParen)) {
        auto expr = tryParseExpr();
        if (!expr.has_value()) {
            error("Expected expression",getLocation());
        }
        tryConsume(TokenType::closeParen, "Expected ')'");
        auto parenExpr = node::term::Paren{.expr = make_unique<Expr>(std::move(expr.value()))};
        return node::Term{.var = std::move(parenExpr)};
    }
    return nullopt;
}

auto Parser::tryParseIfPred() -> unique_ptr<node::IfPred> {
    if (tryConsume(TokenType::kwElif)) {
        auto cond = parseCondition();
        auto scope = parseScope();

        return make_unique<node::IfPred>(
            node::IfPred{
                .var = node::pred::Elif{
                    .cond = std::move(cond),
                    .scope = std::move(scope),
                    .next = tryParseIfPred()
                }
            });
    }
    if (tryConsume(TokenType::kwElse)) {
        return make_unique<node::IfPred>(
            node::IfPred{
                .var = node::pred::Else{
                    .scope = parseScope()
                }
            });
    }
    return nullptr;
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
    if (peek().has_value() && peek().value().type == type) {
        return consume();
    }
    error(errMsg,getLocation());
}
auto Parser::consume() -> Token {
    return _tokens[_index++];
}

auto Parser::getLocation() const -> Location {
    if (peek().has_value()) {
        return peek().value().location;
    }
    return _tokens.empty()? Location{} : _tokens[_index-1].location;
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




