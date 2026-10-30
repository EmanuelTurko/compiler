#pragma once

#include <variant>
#include <vector>
#include <memory>


#include "tokenization.hpp"

using std::optional;
using std::nullopt;
using std::unique_ptr;
using std::make_unique;
using std::variant;


namespace node {
    struct Expr;
    struct Stmt;

    namespace term {
        struct IntLit {
            Token token;
        };

        struct Ident {
            Token token;
        };

        struct Paren {
            unique_ptr<Expr> expr;
        };
    }

    struct Term {
        variant<term::IntLit, term::Ident, term::Paren> var;
    };

    namespace expr {
        struct Add {
            unique_ptr<Expr> lhs;
            unique_ptr<Expr> rhs;
        };

        struct Mul {
            unique_ptr<Expr> lhs;
            unique_ptr<Expr> rhs;
        };

        struct Div {
            unique_ptr<Expr> lhs;
            unique_ptr<Expr> rhs;
        };

        struct Sub {
            unique_ptr<Expr> lhs;
            unique_ptr<Expr> rhs;
        };

        struct Binary {
            variant<Add, Mul, Div, Sub> var;
        };
    }

    struct Expr {
        variant<Term, expr::Binary> var;
    };

    struct Scope {
        std::vector<Stmt> stmts;
    };

    namespace stmt {
        struct Exit {
            Expr expr;
        };

        struct Let {
            Token token;
            Expr expr;
        };

        struct If {
            Expr expr;
            Scope scope;
        };
    }

    struct Stmt {
        variant<stmt::Exit, stmt::Let, Scope, stmt::If> var;
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
    auto parseStmtLet() -> node::stmt::Let;
    auto parseStmtExit() -> node::stmt::Exit;
    auto parseStmtBranchIf() -> node::stmt::If;
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
