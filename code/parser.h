// #ifndef _PARSER_H
// #define _PARSER_H
// #include "AST.h"
// #include "lexer.h"
// #include "llvm/Support/raw_ostream.h"
// #include <string>
// using namespace std;

// class Parser
// {
//     Lexer &Lex;
//     Token Tok;
//     bool HasError;

//     void error()
//     {
//         llvm::errs() << "Unexpected: " << Tok.getText() << "\n";
//         HasError = true;
//     }

//     void advance() { Lex.next(Tok); }

//     bool expect(Token::TokenKind Kind)
//     {
//         if (Tok.getKind() != Kind)
//         {
//             error();
//             return true;
//         }
//         return false;
//     }

//     bool consume(Token::TokenKind Kind)
//     {
//         if (expect(Kind))
//             return true;
//         advance();
//         return false;
//     }

//     // parsing functions according to the regex
//     // pattern specified. each one produces its own node
//     // one node can have multiple subnodes inside it
// public:
//     Base *parse();
//     Base *parseStatement();
//     IfStatement *parseIf();
//     ElseIfStatement *parseElseIf();
//     AssignStatement *parseUnaryExpression(Token &token);
//     Expression *parseExpression();
//     Expression *parseLogicalComparison();
//     Expression *parseIntExpression();
//     Expression *parseTerm();
//     Expression *parseSign();
//     Expression *parsePower();
//     Expression *parseFactor();
//     ForStatement *parseFor();
//     WhileStatement *parseWhile();
//     AssignStatement *parseAssign(llvm::StringRef name);
//     llvm::SmallVector<DecStatement *> parseDefine(Token::TokenKind token_kind);
//     void check_for_semicolon();

// public:
//     // initializes all members and retrieves the first token
//     Parser(Lexer &Lex) : Lex(Lex), HasError(false)
//     {
//         advance();
//     }

//     // get the value of error flag
//     bool hasError() { return HasError; }

// };

// #endif



#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include <memory>
#include <vector>

class Parser {
    Lexer &lexer;
    Token currentTok;
    Token peekTok;

    void advance();
    void expect(Token::TokenKind kind);
    void consume(Token::TokenKind kind);

public:
    Parser(Lexer &lexer);
    std::unique_ptr<ProgramNode> parseProgram();

    // Statement Parsers
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseVarDecl();
    std::unique_ptr<ASTNode> parseIfStatement();
    std::unique_ptr<ASTNode> parseForLoop();
    std::unique_ptr<ASTNode> parseWhileLoop();
    std::unique_ptr<ASTNode> parsePrintStatement();
    std::unique_ptr<ASTNode> parseBlock();
    
    // Expression Parsers
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseLogicalOr();
    std::unique_ptr<ASTNode> parseLogicalAnd();
    std::unique_ptr<ASTNode> parseEquality();
    std::unique_ptr<ASTNode> parseComparison();
    std::unique_ptr<ASTNode> parseTerm();
    std::unique_ptr<ASTNode> parseFactor();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parsePrimary();
    
    // Special
    std::unique_ptr<ASTNode> parseArrayLiteral();
    std::unique_ptr<ASTNode> parseFunctionCall();
};

#endif
