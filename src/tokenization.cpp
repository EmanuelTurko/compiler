#include "tokenization.hpp"


auto Tokenizer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens{};
    
    while (peek().has_value()) {
        if (const auto c = peek().value(); isalpha(c))
            tokens.push_back(tokenizeWord());
        else if (isdigit(c))
            tokens.push_back(tokenizeNum());
        else
            tokenizeSymbol(tokens);
    }
    _index = 0;
    return tokens;
}
auto Tokenizer::peek(const size_t offset) const -> std::optional<char> {
    if (_index + offset >= _src.size()) return std::nullopt;
    return _src[_index + offset];
}
auto Tokenizer::tokenizeWord() -> Token {
    std::string buf{};

    while (peek().has_value() && isalnum(peek().value()))
        buf.push_back(consume());
    if (buf == "exit") return {.type = TokenType::exit};
    if (buf == "let") return {.type = TokenType::let};
    if (buf == "if") return {.type = TokenType::branchIf};
    return {.type = TokenType::ident, .value = buf};
}
auto Tokenizer::tokenizeNum() -> Token {
    std::string buf{};

    while (peek().has_value() && isdigit(peek().value()))
        buf.push_back(consume());
    return {.type = TokenType::intLit, .value = buf};
}

auto Tokenizer::tokenizeSymbol(std::vector<Token> &tokens) -> void {
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
auto Tokenizer::consume() -> char {
    return _src[_index++];
}







