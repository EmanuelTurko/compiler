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
};

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

    const std::string _src;
    size_t _index;
};

inline auto Tokenizer::consume() -> char { return _src[_index++]; }

inline auto Tokenizer::peek(const size_t offset) const -> std::optional<char> {
    if (_index + offset >= _src.size()) return std::nullopt;
    return _src[_index + offset];
}

inline auto Tokenizer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens{};
    std::string buf{};

    while (peek().has_value()) {
        if (isalpha(peek().value())) {
            buf.push_back(consume());
            while (peek().has_value() && isalnum(peek().value())) { buf.push_back(consume()); }
            if (buf == "exit") {
                tokens.push_back({.type = TokenType::exit});
                buf.clear();
                continue;
            }
            else if (buf == "let") {
                tokens.push_back({.type = TokenType::let});
                buf.clear();
                continue;
            }
            else {
                tokens.push_back({.type = TokenType::ident, .value = buf});
                buf.clear();
            }
        }if (isdigit(peek().value())) {
            buf.push_back(consume());
            while (peek().has_value() && isdigit(peek().value())) { buf.push_back(consume()); }
            tokens.push_back({.type = TokenType::intLit, .value = buf});
            buf.clear();
        } else if (peek().value() == '(') {
            consume();
            tokens.push_back({.type = TokenType::openParen});
        } else if (peek().value() == ')') {
            consume();
            tokens.push_back({.type = TokenType::closeParen});
        } else if (peek().value() == ';') {
            tokens.push_back({.type = TokenType::semi});
            consume();
        } else if (peek().value() == '=') {
            tokens.push_back({.type = TokenType::eq});
            consume();
        } else if (peek().value() == '+'){
            tokens.push_back({.type = TokenType::plus});
            consume();
        } else if (isspace(
            peek().value())) { consume(); } else {
            std::cerr << "You messed up!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    _index = 0;
    return tokens;
}
