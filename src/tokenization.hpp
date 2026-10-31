#pragma once

#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <optional>
#include <unordered_map>

#include "location.hpp"

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
    branchElif,
    branchElse,
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
    Location location{};


};

class Tokenizer {
public:
    explicit Tokenizer(std::string src) : _src(std::move(src))
                                                                  {
    }

    [[nodiscard]] auto tokenize() -> std::vector<Token>;

private:
    [[nodiscard]] auto peek(size_t offset = 0) const -> std::optional<char>;

    [[nodiscard]] auto tokenizeWord() -> Token;

    [[nodiscard]] auto tokenizeNum() -> Token;

    [[nodiscard]] auto skipComment() -> bool;

    auto tokenizeSymbol(std::vector<Token> &tokens, Location location) -> void;

    auto consume() -> char;

    const std::string _src;
    size_t _index{};
    Location _location{};


    inline static const std::unordered_map<std::string_view, TokenType> _keywords= {
        {"exit", TokenType::exit},
        {"let", TokenType::let},
        {"if", TokenType::branchIf},
        {"elif", TokenType::branchElif},
        {"else", TokenType::branchElse}
    };
    inline static const std::unordered_map<char, TokenType> _symbols= {
        {'(', TokenType::openParen},
        {')', TokenType::closeParen},
        {'{', TokenType::openCurly},
        {'}', TokenType::closeCurly},
        {'=', TokenType::eq},
        {'+', TokenType::plus},
        {'*', TokenType::mul},
        {'-', TokenType::sub},
        {'/', TokenType::div},
        {';', TokenType::semi}
    };
};
