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

    namespace literal {
        struct Int {
            Token token;
        };

        struct Bool {
            Token token;
        };

        struct String {
            Token token;
        };
    }

    struct Literal {
        variant<literal::Int, literal::Bool, literal::String> var;
    };

    namespace expr {
        struct Unary {
            Token op;
            unique_ptr<Expr> expr;
        };

        struct Binary {
            Token op;
            unique_ptr<Expr> lhs;
            unique_ptr<Expr> rhs;
        };

        struct Ident {
            Token token;
        };

        struct Call {
            expr::Ident funName;
            std::vector<Expr> args;
        };
    }

    struct Expr {
        variant<Literal, expr::Ident, expr::Binary, expr::Unary, expr::Call> var;
    };

    struct Block {
        std::vector<Stmt> stmts;
    };

    struct Type {
        Token token;
    };

    namespace pred {
        struct Elif {
            Expr cond;
            Block block;
            unique_ptr<IfPred> next;
        };

        struct Else {
            Block block;
        };
    }

    struct IfPred {
        variant<pred::Elif, pred::Else> var;
    };

    namespace stmt {
        struct Ret {
            std::optional<Expr> expr;
        };

        struct Def {
            std::optional<Type> type;
            expr::Ident ident;
            std::optional<Expr> expr;
        };

        struct If {
            Expr cond;
            Block block;
            unique_ptr<IfPred> next;
        };

        struct Assign {
            expr::Ident ident;
            Expr expr;
        };

        struct While {
            Expr cond;
            Block block;
        };

        struct ExprStmt {
            Expr expr;
        };
    }

    struct Stmt {
        variant<stmt::Ret, stmt::Def, Block, stmt::If, stmt::Assign, stmt::While, stmt::ExprStmt> var;
    };

    struct Param {
        Type type;
        expr::Ident ident;
    };

    struct Fun {
        expr::Ident ident;
        std::vector<Param> params; // params+ (int x,int y)
        std::optional<Type> retType; // -> bool
        Block block;
    };

    struct Program {
        std::vector<Fun> functions;
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
    auto parseScope() -> node::Block;
    auto parseStmtLet() -> node::stmt::Def;
    auto parseStmtRet() -> node::stmt::Ret;
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
