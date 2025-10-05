#pragma once
#include <string>
#include "error_handler.h"

struct Token {
    enum Kind {
        End, Identifier, Number, KwInt, KwReturn, Plus, Minus, Star, Slash, LParen, RParen, LBrace, RBrace, Semicolon, Assign
    } kind = End;
    std::string lexeme;
    SourceLocation loc;
};

class Lexer {
public:
    explicit Lexer(const std::string& input);
    Token next();
private:
    const std::string input;
    size_t index = 0;
    int line = 1;
    int column = 1;
};
