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
    //literals and names
    litInt,ident,litString,litBool,

    //keywords
    kwIf,kwElif,kwElse,kwWhile,kwRet,kwDef,kwFun,kwInt,kwBool,kwString,kwTrue,kwFalse,

    // punctuation
    openParen,closeParen,openCurly,closeCurly,comma,semi,arrow,

    //operators
    assign,plus,star,minus,slash,eq,neq,less,greater,greaterEq,lessEq,logicalAnd,logicalOr,logicalNot,inc,dec,
    percent
};

inline auto binPrecedence(const TokenType &type) -> std::optional<int> {
    switch (type) {
        case TokenType::assign:
            return 0;
        case TokenType::logicalOr:
            return 1;
        case TokenType::logicalAnd:
            return 2;
        case TokenType::eq:
        case TokenType::neq:
            return 3;
        case TokenType::less:
           case TokenType::greater:
        case TokenType::lessEq:
            case TokenType::greaterEq:
            return 4;
        case TokenType::plus:
        case TokenType::minus:
            return 5;
        case TokenType::slash:
        case TokenType::star:
        case TokenType::percent:
            return 6;
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

    [[nodiscard]] auto trySkipComment() -> bool;

    auto tryTokenizeDoubleSymbol(std::vector<Token>& tokens,Location location) -> bool;
    auto tokenizeSymbol(std::vector<Token> &tokens, Location location) -> void;

    auto consume() -> char;

    const std::string _src;
    size_t _index{};
    Location _location{};


    inline static const std::unordered_map<std::string_view, TokenType> _keywords= {
        {"ret", TokenType::kwRet},
        {"def", TokenType::kwDef},
        {"if", TokenType::kwIf},
        {"elif", TokenType::kwElif},
        {"else", TokenType::kwElse}
    };
    inline static const std::unordered_map<char, TokenType> _symbols= {
        {'(', TokenType::openParen},
        {')', TokenType::closeParen},
        {'{', TokenType::openCurly},
        {'}', TokenType::closeCurly},
        {'=', TokenType::assign},
        {'+', TokenType::plus},
        {'*', TokenType::star},
        {'-', TokenType::minus},
        {'/', TokenType::slash},
        {';', TokenType::semi},
        {',', TokenType::comma},
        {'%', TokenType::percent},
        {'<', TokenType::less},
        {'>', TokenType::greater},
        {'!', TokenType::logicalNot},
    };
    inline static const std::unordered_map<std::string_view,TokenType> _doubleSymbols= {
        {"&&", TokenType::logicalAnd},
        {"||", TokenType::logicalOr},
        {"==", TokenType::eq},
        {"!=", TokenType::neq},
        {"<=", TokenType::lessEq},
        {">=", TokenType::greaterEq},
        {"->", TokenType::arrow},
        {"++",TokenType::inc},
        {"--",TokenType::dec}
    };
};
