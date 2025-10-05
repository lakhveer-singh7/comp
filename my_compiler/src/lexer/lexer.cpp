#include "lexer.h"
#include <cctype>

namespace {
    bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    bool isIdentPart(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
}

Lexer::Lexer(const std::string& src) : input(src) {}

Token Lexer::next() {
    // Skip whitespace
    while (index < input.size()) {
        char c = input[index];
        if (c == ' ' || c == '\t' || c == '\r') {
            ++index; ++column; continue;
        }
        if (c == '\n') { ++index; ++line; column = 1; continue; }
        break;
    }

    if (index >= input.size()) {
        Token t; t.kind = Token::End; t.lexeme = ""; t.loc = {line, column}; return t;
    }

    const size_t startIndex = index;
    const int startLine = line;
    const int startColumn = column;
    char c = input[index];

    // Numbers (decimal integers)
    if (std::isdigit(static_cast<unsigned char>(c))) {
        while (index < input.size() && std::isdigit(static_cast<unsigned char>(input[index]))) {
            ++index; ++column;
        }
        Token t; t.kind = Token::Number; t.lexeme = input.substr(startIndex, index - startIndex); t.loc = {startLine, startColumn};
        return t;
    }

    // Identifiers / keywords
    if (isIdentStart(c)) {
        ++index; ++column;
        while (index < input.size() && isIdentPart(input[index])) { ++index; ++column; }
        std::string lex = input.substr(startIndex, index - startIndex);
        Token t; t.lexeme = lex; t.loc = {startLine, startColumn};
        if (lex == "int") t.kind = Token::KwInt;
        else if (lex == "return") t.kind = Token::KwReturn;
        else t.kind = Token::Identifier;
        return t;
    }

    // Single-character tokens
    ++index; ++column;
    Token t; t.lexeme = std::string(1, c); t.loc = {startLine, startColumn};
    switch (c) {
        case '+': t.kind = Token::Plus; break;
        case '-': t.kind = Token::Minus; break;
        case '*': t.kind = Token::Star; break;
        case '/': t.kind = Token::Slash; break;
        case '(': t.kind = Token::LParen; break;
        case ')': t.kind = Token::RParen; break;
        case '{': t.kind = Token::LBrace; break;
        case '}': t.kind = Token::RBrace; break;
        case ';': t.kind = Token::Semicolon; break;
        case '=': t.kind = Token::Assign; break;
        default:
            // Unknown characters are ignored for now; return End to avoid infinite loop
            t.kind = Token::End; break;
    }
    return t;
}
