#include "tokenization.hpp"

#include "error.hpp"

/** Turns the raw source text into a flat list of tokens. One pass, left to right, never goes back.
 *
 *  1. While there are characters left, peek at the current one (don't consume yet) and decide by its first char:
 *  1.1 Letter -> tokenizeWord(): consume letters/digits into a buffer while they keep coming.
 *      If the buffer is a keyword (exit, let, if) -> keyword token, no value.
 *      Otherwise -> ident token, the buffer is its value (the variable name).
 *  1.2 Digit  -> tokenizeNum(): consume digits into a buffer. -> intLit token, the digits are its value (kept as text).
 *  1.3 Anything else -> tokenizeSymbol(): one char, one token ( ( ) { } ; = + - * / ).
 *      Whitespace (' ', \n, \t, \r) is consumed but produces no token.
 *      Unknown char -> error and exit.
 *  2. The helpers stop on the first char that doesn't belong to them, without consuming it,
 *     so the next loop iteration starts exactly on it ("x=5" works without spaces).
 *  3. Reset _index to 0 and return the tokens.
 *
 *  Example: let x = (10 - 2);
 *  'l' letter -> word: "let"  -> keyword        [let]
 *  ' ' space  -> skipped
 *  'x' letter -> word: "x"    -> not a keyword  [ident "x"]
 *  ' ' skipped, '=' symbol                      [eq]
 *  ' ' skipped, '(' symbol                      [openParen]
 *  '1' digit  -> num: "10", stops at ' '        [intLit "10"]
 *  ' ' skipped, '-' symbol, ' ' skipped         [minus]
 *  '2' digit  -> num: "2", stops at ')'         [intLit "2"]
 *  ')' symbol                                   [closeParen]
 *  ';' symbol                                   [semi]
 *  end of source -> loop ends.
 *
 *  Result: [let] [ident "x"] [eq] [openParen] [intLit "10"] [minus] [intLit "2"] [closeParen] [semi]
 *
 *  Note: words may contain digits after the first letter ("x2" is one ident),
 *        but a word can't start with a digit: "2x" becomes [intLit "2"] [ident "x"].
 * @return all tokens of the source, in order
 */
auto Tokenizer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens{};

    while (peek().has_value()) {
        const auto startingLocation = _location;
        if (const auto c = peek().value(); isalpha(c)) {
            auto token = tokenizeWord();
            token.location = startingLocation;
            tokens.push_back(token);
        }
        else if (isdigit(c)) {
            auto token = tokenizeNum();
            token.location = startingLocation;
            tokens.push_back(token);
        }
        else if (skipComment())
            continue;
        else {

        }
            tokenizeSymbol(tokens,startingLocation);
    }
    _index = 0;
    _location.reset();
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

    if (const auto it = _keywords.find(buf); it != _keywords.cend())
        return {.type = it->second};
    return {.type = TokenType::ident, .value = buf};
}
auto Tokenizer::tokenizeNum() -> Token {
    std::string buf{};

    while (peek().has_value() && isdigit(peek().value()))
        buf.push_back(consume());
    return {.type = TokenType::intLit, .value = buf};
}

auto Tokenizer::tokenizeSymbol(std::vector<Token> &tokens, const Location location) -> void {
    if (const auto val = peek().value(); std::isspace(val)) {
        consume();
        return;
    }
    else if (const auto it = _symbols.find(val); it != _symbols.cend()) {
        tokens.push_back({.type = it->second, .value = {}, .location = location});
        consume();
        return;
    }
    error("Unexpected symbol",location);
}
auto Tokenizer::consume() -> char {
    const auto c = _src[_index];
    _location.advance(c);
    _index++;
    return c;
}

auto Tokenizer::skipComment() -> bool {
    if (peek().value() == '/') {
        if (peek(1).has_value() && peek(1).value() == '/') {
            consume();
            consume();
            while (peek().has_value() && peek().value() != '\n') consume();
            return true;
        }
        if (peek(1).has_value() && peek(1).value() == '*') {
            consume();
            consume();
            while (peek().has_value() && peek(1).has_value()) {
                if (peek().value() == '*' && peek(1).value() == '/') {
                    consume();
                    consume();
                    return true;
                }
                consume();
            }
            error("Unexpected end of file",_location);
        }
    }
    return false;
}



