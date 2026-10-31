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
    struct IfPred;

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
    namespace pred {
        struct Elif {
            Expr cond;
            Scope scope;
            unique_ptr<IfPred> next;
        };
        struct Else {
            Scope scope;
        };
    }
    struct IfPred {
        variant<pred::Elif,pred::Else> var;
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
            Expr cond;
            Scope scope;
            unique_ptr<IfPred> next;
        };
        struct Assignment {
            Token token;
            Expr expr;
        };
    }
    struct Stmt {
        variant<stmt::Exit, stmt::Let, Scope, stmt::If,stmt::Assignment> var;
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
    auto parseScope() -> node::Scope;
    auto parseStmtLet() -> node::stmt::Let;
    auto parseStmtExit() -> node::stmt::Exit;
    auto parseStmtBranchIf() -> node::stmt::If;
    auto parseCondition() -> node::Expr;
    auto parseAssignment() -> node::stmt::Assignment;
    auto parseExpr() -> node::Expr;
    auto tryParseExpr(int minPrecedence = 0) -> optional<node::Expr>;
    auto tryParseTerm() -> optional<node::Term>;
    auto tryParseIfPred() -> unique_ptr<node::IfPred>;

    auto tryConsume(TokenType type) -> optional<Token>;
    auto tryConsume(TokenType type, const std::string &errMsg) -> Token;
    auto consume() -> Token;


    [[nodiscard]] auto peek(size_t offset = 0) const -> optional<Token>;
    [[nodiscard]] auto getLocation() const -> Location;
    template<typename T>
    [[nodiscard]] auto constructExpr(node::Expr lhs,node::Expr rhs) -> node::Expr;

    const std::vector<Token> _tokens;
    size_t _index;
};
