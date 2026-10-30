#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <optional>

using std::isalpha;
using std::isalnum;
using std::isdigit;
using std::isspace;

enum class TokenType {
    exit,
    intLit,
    semi,
    openParen,
    closeParen,
    ident,
    let,
    eq,
    plus,
    mul,
    sub,
    div,
    openCurly,
    closeCurly,
    branchIf,
};

inline auto isBinOp(const TokenType &type) -> bool {
    switch (type) {
        case TokenType::plus:
        case TokenType::mul:
        case TokenType::sub:
        case TokenType::div:
            return true;
        default:
            return false;
    }
}

inline auto binPrecedence(const TokenType &type) -> std::optional<int> {
    switch (type) {
        case TokenType::plus:
        case TokenType::sub:
            return 0;
        case TokenType::mul:
        case TokenType::div:
            return 1;
        default:
            return std::nullopt;
    }
}

struct Token {
    TokenType type{};
    std::optional<std::string> value{};
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string &src, const int index = 0) : _src(std::move(src)),
                                                                      _index(index) {
    }

    [[nodiscard]] auto tokenize() -> std::vector<Token>;

private:
    [[nodiscard]] auto peek(size_t offset = 0) const -> std::optional<char>;

    [[nodiscard]] auto tokenizeWord() -> Token;

    [[nodiscard]] auto tokenizeNum() -> Token;

    [[nodiscard]] auto skipComment() -> bool;

    auto tokenizeSymbol(std::vector<Token> &tokens) -> void;

    auto consume() -> char;


    const std::string _src;
    size_t _index;
};
