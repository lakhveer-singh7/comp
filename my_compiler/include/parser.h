#pragma once
#include <memory>
#include <vector>
#include <string>
#include "lexer.h"
#include "ast.h"
#include "error_handler.h"

class Parser {
public:
    Parser(Lexer& lexer, ErrorHandler& err) : lexer(lexer), err(err) {
        current = this->lexer.next();
    }

    std::unique_ptr<Function> parseFunction();

private:
    Lexer& lexer;
    ErrorHandler& err;
    Token current;

    void advance() { current = lexer.next(); }
    bool accept(Token::Kind k) { if (current.kind == k) { advance(); return true; } return false; }
    bool expect(Token::Kind k, const char* what);

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseTerm();
    std::unique_ptr<Expr> parseFactor();
};
