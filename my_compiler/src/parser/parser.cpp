#include "parser.h"
#include <cstdlib>

bool Parser::expect(Token::Kind k, const char* what) {
    if (current.kind != k) {
        err.report(current.loc, std::string("expected ") + what + ", found '" + current.lexeme + "'");
        return false;
    }
    advance();
    return true;
}

std::unique_ptr<Expr> Parser::parseFactor() {
    if (current.kind == Token::Number) {
        long val = std::strtol(current.lexeme.c_str(), nullptr, 10);
        advance();
        return std::make_unique<NumberExpr>(val);
    }
    if (current.kind == Token::LParen) {
        advance();
        auto e = parseExpression();
        expect(Token::RParen, ")");
        return e;
    }
    err.report(current.loc, "expected number or '('");
    return std::make_unique<NumberExpr>(0);
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto left = parseFactor();
    while (current.kind == Token::Star || current.kind == Token::Slash) {
        char op = (current.kind == Token::Star ? '*' : '/');
        advance();
        auto right = parseFactor();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseExpression() {
    auto left = parseTerm();
    while (current.kind == Token::Plus || current.kind == Token::Minus) {
        char op = (current.kind == Token::Plus ? '+' : '-');
        advance();
        auto right = parseTerm();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Function> Parser::parseFunction() {
    // int <identifier>() { return <expr>; }
    if (!expect(Token::KwInt, "'int'")) return nullptr;
    if (current.kind != Token::Identifier) {
        err.report(current.loc, "expected function name");
        return nullptr;
    }
    std::string name = current.lexeme;
    advance();
    expect(Token::LParen, "'('");
    expect(Token::RParen, "')'");
    expect(Token::LBrace, "'{' ");
    if (!expect(Token::KwReturn, "'return'")) return nullptr;
    auto expr = parseExpression();
    expect(Token::Semicolon, "';'");
    expect(Token::RBrace, "'}'");
    auto fn = std::make_unique<Function>();
    fn->name = name;
    fn->body.emplace_back(std::make_unique<ReturnStmt>(std::move(expr)));
    return fn;
}
