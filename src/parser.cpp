#include "parser.hpp"

#include <algorithm>

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
auto Parser::parseProgram() -> node::Program {
    node::Program program;
    while (peek().has_value()) {
        auto fun = parseFun();
        program.functions.push_back(std::move(fun));
    }
    return program;
}
auto Parser::parseFun() -> node::Fun { // fun myfun(int x,int y) -> bool { }
    tryConsume(TokenType::kwFun, "expected function keyword");
    const auto funName = tryConsume(TokenType::ident,"Expected identifier");

    tryConsume(TokenType::openParen, "expected '('");
    auto params = parseParams();
    tryConsume(TokenType::closeParen, "expected ')'");

    std::optional<node::Type> type{};
    if (peekIs(TokenType::arrow)) {
        // If the return value isn't void: fun(x) -> bool
        consume();
        type = parseType();
    }
    auto block = parseBlock();
    return node::Fun{
        .name = node::expr::Ident{funName},
        .params = std::move(params),
        .retType = type,
        .block =std::move(block)
    };
}
auto Parser::parseParams() -> std::vector<node::Param> {
    // params are built on (Type identifier,Type identifier,..) (int x,int y)
    if (peekIs(TokenType::closeParen))
        return {};

    std::vector<node::Param> params;
    while (true) {
        const auto type = parseType();
        const auto ident = tryConsume(TokenType::ident,"expected identifier");
        params.push_back({
            .type = type,
            .ident = node::expr::Ident{.token = ident}});

        if (peekIs(TokenType::comma))
            consume();
        else if (peekIs(TokenType::closeParen)) break;
        else error("Expected ',' or ')'", getLocation());
    }
    return params;
}
auto Parser::parseType() -> node::Type {
    if (isTypeKeyword())
        return node::Type{.token = consume()};
    error("Expected a type",getLocation());
}
auto Parser::isTypeKeyword() const -> bool {
   return peekIs(TokenType::kwBool) || peekIs(TokenType::kwInt) || peekIs(TokenType::kwString);
}
auto Parser::tryParseStmt() -> optional<Stmt> {
    if (!peek().has_value())
        return nullopt;

    if (peekIs(TokenType::openCurly))
        return Stmt{.var = parseBlock()};
    if (peekIs(TokenType::kwIf))
        return Stmt{.var = parseIf()};
    if (peekIs(TokenType::kwWhile))
        return Stmt{.var = parseWhile()};
    if (peekIs(TokenType::kwRet))
        return Stmt{.var = parseRet()};
    if (peekIs(TokenType::kwDef))
        return Stmt{.var = parseDef()};
    if (peekIs(TokenType::ident) && peekIs(TokenType::assign,1))
        return Stmt{.var = parseAssign()};
    if (peekIs(TokenType::ident) && peekIs(TokenType::openParen,1))
        return Stmt{.var = parseExprStmt()};

    return nullopt;
}
auto Parser::parseBlock() -> node::Block {
    node::Block block{};

    tryConsume(TokenType::openCurly,"Expected '{'");
    while (true) // recursion to parse the entire scoped stmt { | def x = 5; | }
    {
        if (peekIs(TokenType::closeCurly)) break;
        if (auto stmt = tryParseStmt();
            stmt.has_value()) block.stmts.push_back(std::move(stmt.value()));
        else error("Expected a statement",getLocation());
    }
    tryConsume(TokenType::closeCurly, "expected '}'");
    return block;
}
auto Parser::parseDef() -> node::stmt::Def {
    // def int x = 5;
    // def x = y;
    // def int x;
    // def x; <-- not valid
    std::optional<node::Type> type{};
    std::optional<Expr> expr{};

    tryConsume(TokenType::kwDef,"expected keyword");
    if (isTypeKeyword()) type = parseType();
    const auto ident = tryConsume(TokenType::ident, "expected identifier");
    if (peekIs(TokenType::assign)) {
        consume();
        expr = parseExpr();
    }
    if (!type.has_value() && !expr.has_value()) error("def needs a type or a value",ident.location);
    tryConsume(TokenType::semi, "expected ';'");

    return node::stmt::Def{
        .type = type,
        .ident = node::expr::Ident{.token = ident},
        .expr = std::move(expr)
    };
}
auto Parser::parseRet() -> node::stmt::Ret {
    // ret x; <-- type
    // ret true; <-- keyword
    // ret; <-- void
    std::optional<Expr> expr{};

    tryConsume(TokenType::kwRet, "expected ret");
    if (!peekIs(TokenType::semi)) expr = parseExpr();
    tryConsume(TokenType::semi, "expected ';'");
    return node::stmt::Ret{.expr = std::move(expr)};
}
auto Parser::parseIf() -> node::stmt::If {
    // if(x > 5) { do stuff; }
    tryConsume(TokenType::kwIf, "expected 'if'");
    auto cond = parseCondition();

    auto block = parseBlock();
    auto next = tryParseIfPred();
    return node::stmt::If{
        .cond = std::move(cond),
        .block = std::move(block),
        .next = std::move(next)
    };
}

auto Parser::parseWhile() -> node::stmt::While {
    tryConsume(TokenType::kwWhile, "expected 'while'");
    auto cond = parseCondition();
    auto block = parseBlock();
    return node::stmt::While{
        .cond = std::move(cond),
        .block = std::move(block)
    };
}

auto Parser::parseCondition() -> node::Expr {
    tryConsume(TokenType::openParen, "Expected '('");
    auto cond = parseExpr();
    tryConsume(TokenType::closeParen, "Expected ')'");

    return cond;
}

auto Parser::parseAssign() -> node::stmt::Assign {
    // x = 5;
    // x = y;
    const auto token = tryConsume(TokenType::ident,"expected identifier");
    tryConsume(TokenType::assign, "Expected '='");
    auto expr = parseExpr();
    tryConsume(TokenType::semi, "Expected ';'");
    return {
        .ident = node::expr::Ident{.token = token},
        .expr = std::move(expr)
    };
}

auto Parser::parseExprStmt() -> node::stmt::ExprStmt {
    auto exprStmt = node::stmt::ExprStmt{.call = parseCall()};
    tryConsume(TokenType::semi,"Expected ';'");
    return exprStmt;
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

    Expr lhs = parsePrimary();
    while (true) {
        int precedence{-1}; // no op
        if (peek().has_value()) precedence = binPrecedence(peek().value().type);
        if (precedence < minPrecedence) break;

        const auto token = consume();
        auto rhs = tryParseExpr(precedence+1);
        if (!rhs.has_value())
            error("Unable to parse expression",getLocation());
        lhs.var = node::expr::Binary{
            .op = token,
            .lhs = make_unique<Expr>(std::move(lhs)),
            .rhs = make_unique<Expr>(std::move(rhs.value()))
        };
    }
    return lhs;
}

auto Parser::parseCall() -> node::expr::Call {
    std::vector<Expr> args{};
    const auto token = tryConsume(TokenType::ident, "expected function name");
    tryConsume(TokenType::openParen, "expected '('");
    if (!peekIs(TokenType::closeParen)) {
        while (true) {
            args.push_back(parseExpr());
            if (peekIs(TokenType::closeParen)) break;
            if (peekIs(TokenType::comma)) consume();
            else error("expected ',' or ')'", getLocation());
        }
    }
    tryConsume(TokenType::closeParen, "expected ')'");
    return
        node::expr::Call{
            .funName = node::expr::Ident{.token = token},
            .args = std::move(args)
        };
}

auto Parser::tryParseIfPred() -> unique_ptr<node::IfPred> {
    if (tryConsume(TokenType::kwElif)) {
        auto cond = parseCondition();
        auto block = parseBlock();

        return make_unique<node::IfPred>(
            node::IfPred{
                .var = node::pred::Elif{
                    .cond = std::move(cond),
                    .block = std::move(block),
                    .next = tryParseIfPred()
                }
            });
    }
    if (tryConsume(TokenType::kwElse)) {
        return make_unique<node::IfPred>(
            node::IfPred{
                .var = node::pred::Else{
                    .block = parseBlock()
                }
            });
    }
    return nullptr;
}

auto Parser::peek(const size_t offset) const -> optional<Token> {
    if (_index + offset >= _tokens.size()) return std::nullopt;
    return _tokens[_index + offset];
}

auto Parser::peekIs(const TokenType& type,const size_t offset) const -> bool {
    return peek(offset).has_value() && peek(offset).value().type == type;
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

auto Parser::parsePrimary() -> node::Expr {
    Expr expr{};

    if (peekIs(TokenType::litInt))
        expr.var = node::Literal{.var = node::literal::Int{.token = consume()}};
    else if (peekIs(TokenType::kwTrue) || peekIs(TokenType::kwFalse))
        expr.var = node::Literal{.var = node::literal::Bool{.token = consume()}};
    else if (peekIs(TokenType::ident) && peekIs(TokenType::openParen, 1))
        expr.var = node::expr::Call{parseCall()};
    else if (peekIs(TokenType::ident))
        expr.var = node::expr::Ident{.token = consume()};
    else if (peekIs(TokenType::minus) || peekIs(TokenType::logicalNot))
        expr.var = node::expr::Unary{.op = consume(), .expr = make_unique<node::Expr>(parsePrimary())};
    else if (peekIs(TokenType::openParen)) {
        consume();
        expr = parseExpr();
        tryConsume(TokenType::closeParen, "expected ')'");
    } else
        error("Expected expression", getLocation());
    return expr;
}




