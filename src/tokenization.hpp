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
    minus,
    div,
    openCurly,
    closeCurly,
    branchIf,

};
inline auto isBinOp(const TokenType& type) ->bool {
    switch (type) {
        case TokenType::plus:
        case TokenType::mul:
        case TokenType::minus:
        case TokenType::div:
            return true;
        default:
            return false;
    }
}

inline auto binPrecedence(const TokenType &type) -> std::optional<int> {
    switch (type) {
        case TokenType::plus:
        case TokenType::minus:
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

    auto tokenize() -> std::vector<Token>;

    auto consume() -> char;

private:
    [[nodiscard]] auto peek(size_t offset = 0) const -> std::optional<char>;
    [[nodiscard]] auto tokenizeWord() -> Token;
    [[nodiscard]] auto tokenizeNum() -> Token;
    auto tokenizeSymbol(std::vector<Token>& tokens) -> void;
    const std::string _src;
    size_t _index;
};

inline auto Tokenizer::consume() -> char {
    return _src[_index++];
}

inline auto Tokenizer::peek(const size_t offset) const -> std::optional<char> {
    if (_index + offset >= _src.size()) return std::nullopt;
    return _src[_index + offset];
}

inline auto Tokenizer::tokenizeNum() -> Token {
    std::string buf{};

    while (peek().has_value() && isdigit(peek().value()))
        buf.push_back(consume());
    return {.type = TokenType::intLit, .value = buf};
}

inline auto Tokenizer::tokenizeWord() -> Token {
    std::string buf{};

    while (peek().has_value() && isalnum(peek().value()))
        buf.push_back(consume());
    if (buf == "exit") return {.type = TokenType::exit};
    if (buf == "let") return {.type = TokenType::let};
    if (buf == "if") return {.type = TokenType::branchIf};
    return {.type = TokenType::ident, .value = buf};
}

inline auto Tokenizer::tokenizeSymbol(std::vector<Token> &tokens) -> void {
    switch (peek().value()) {
        case '(': tokens.push_back({.type = TokenType::openParen});
            break;
        case ')': tokens.push_back({.type = TokenType::closeParen});
            break;
        case '{': tokens.push_back({.type = TokenType::openCurly});
            break;
        case '}': tokens.push_back({.type = TokenType::closeCurly});
            break;
        /*case '[': tokens.push_back({.type = TokenType::openSquare});
            break;
        case ']': tokens.push_back({.type = TokenType::closeSquare});
            break;*/
        case ';': tokens.push_back({.type = TokenType::semi});
            break;
        case '=': tokens.push_back({.type = TokenType::eq});
            break;
        case '+': tokens.push_back({.type = TokenType::plus});
            break;
        case '*': tokens.push_back({.type = TokenType::mul});
            break;
        case '-': tokens.push_back({.type = TokenType::minus});
            break;
        case '/': tokens.push_back({.type = TokenType::div});
            break;
        case ' ':
        case '\n':
        case '\t':
        case '\r': break;
        default:
            std::cerr << "Unexpected symbol: " << peek().value() << std::endl;
            exit(EXIT_FAILURE);
    }
    consume();
}

inline auto Tokenizer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens{};

    while (peek().has_value()) {
        if (isalpha(peek().value()))
            tokens.push_back(tokenizeWord());
        else if (isdigit(peek().value()))
            tokens.push_back(tokenizeNum());
        else
            tokenizeSymbol(tokens);
    }
    _index = 0;
    return tokens;
}
