#include "tokenization.hpp"

#include <charconv>
#include <cstdint>

#include "error.hpp"

/** Turns the raw source text into a flat list of tokens. One pass, left to right, never goes back.
 *
 *  A Token is {type, value, location}:
 *    type     - what it is (TokenType: let, ident, intLit, plus, semi, ...)
 *    value    - only for ident (the name) and intLit (the digits, kept as text), empty otherwise
 *    location - line:col of the token's FIRST char, used by every error message (see error.hpp)
 *  _location tracks the current line:col; consume() is the only place that moves it.
 *
 *  1. While there are characters left, save where we are (startingLocation), peek at the current char
 *     (don't consume yet) and decide by it:
 *  1.1 Letter -> tokenizeWord(): consume letters/digits into a buffer while they keep coming.
 *      If the buffer is a keyword (exit, let, if, elif, else) -> keyword token, no value.
 *      Otherwise -> ident token, the buffer is its value (the variable name).
 *  1.2 Digit  -> tokenizeNum(): consume digits into a buffer -> intLit token, the digits are its value.
 *      Numbers that don't fit in a signed 64-bit int are rejected here.
 *  1.3 '//' or '/' '*' -> skipComment(): consume up to the end of the line / the closing '*' '/'. No token.
 *      An unclosed block comment is an error.
 *  1.4 Anything else -> tokenizeSymbol(): whitespace is consumed with no token;
 *      a known symbol ( ( ) { } ; = + - * / ) becomes one token (via the _symbols table); anything else is an error.
 *  2. The token gets startingLocation, so errors point at its first char, not at where reading stopped.
 *  3. The helpers stop on the first char that doesn't belong to them, without consuming it,
 *     so the next loop iteration starts exactly on it ("x=5" works without spaces).
 *  4. Reset _index and _location and return the tokens.
 *
 *  Example: let x = (10 - 2); // ten minus two
 *  'l' letter -> word: "let"  -> keyword        [let]                 1:1
 *  ' ' space  -> skipped
 *  'x' letter -> word: "x"    -> not a keyword  [ident "x"]           1:5
 *  ' ' skipped, '=' symbol                      [eq]                  1:7
 *  ' ' skipped, '(' symbol                      [openParen]           1:9
 *  '1' digit  -> num: "10", stops at ' '        [intLit "10"]         1:10
 *  ' ' skipped, '-' symbol, ' ' skipped         [sub]                 1:13
 *  '2' digit  -> num: "2", stops at ')'         [intLit "2"]          1:15
 *  ')' symbol                                   [closeParen]          1:16
 *  ';' symbol                                   [semi]                1:17
 *  ' ' skipped, '//' comment -> skipped to end of line
 *  end of source -> loop ends.
 *
 *  Result: [let] [ident "x"] [eq] [openParen] [intLit "10"] [sub] [intLit "2"] [closeParen] [semi]
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
        else if (trySkipComment()) continue;
        else if (tryTokenizeDoubleSymbol(tokens,startingLocation)) continue;
        else tokenizeSymbol(tokens,startingLocation);
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
    const auto startLocation = _location;

    while (peek().has_value() && isdigit(peek().value()))
        buf.push_back(consume());

    std::int64_t parsed{};
    if (const auto returnVal = std::from_chars(buf.data(),buf.data()+buf.size(),parsed);
        returnVal.ec == std::errc()) {
        return {.type = TokenType::litInt, .value = buf};
    }
    error("integer overflow",startLocation);
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

auto Tokenizer::trySkipComment() -> bool {
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

auto Tokenizer::tryTokenizeDoubleSymbol(std::vector<Token>& tokens, Location location) -> bool {
    if (peek(1).has_value()) {
       const auto s = std::string_view(_src).substr(_index,2);

        if (const auto it = _doubleSymbols.find(s); it != _doubleSymbols.cend()) {
            tokens.push_back({.type = it->second, .value = {}, .location = location});
            consume();
            consume();
            return true;
        }
    }
        return false;
}



