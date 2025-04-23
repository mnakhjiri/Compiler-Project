#include "parser.h"
#include <stdexcept>
#include <iostream>
#include <memory>

Parser::Parser(Lexer &lexer) : lexer(lexer) {
    advance();
    advance();
}

void Parser::advance() {
    currentTok = peekTok;
    peekTok = lexer.nextToken();
}

void Parser::expect(Token::TokenKind kind) {
    if (!currentTok.is(kind)) {
        throw std::runtime_error("Syntax Error: Expected " + 
                                tokenToString(kind) + 
                                " but found " + 
                                tokenToString(currentTok.kind) +
                                " at line " + std::to_string(currentTok.line));
    }
}

void Parser::consume(Token::TokenKind kind) {
    expect(kind);
    advance();
}

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    auto program = std::make_unique<ProgramNode>();
    while (!currentTok.is(Token::eof)) {
        program->statements.push_back(parseStatement());
    }
    return program;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    switch (currentTok.kind) {
        case Token::KW_int:
        case Token::KW_bool:
        case Token::KW_float:
        case Token::KW_char:
        case Token::KW_string:
        case Token::KW_array:
            return parseVarDecl();
            
        case Token::KW_if:
            return parseIfStatement();
            
        case Token::KW_for:
            return parseForLoop();
            
        case Token::KW_while:
            return parseWhileLoop();
            
        case Token::KW_print:
            return parsePrintStatement();
            
        case Token::l_brace:
            return parseBlock();
            
        default:
            return parseExpressionStatement();
    }
}

// Variable Declaration
std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    VarType type = tokenToVarType(currentTok.kind);
    advance();
    
    std::vector<VarDecl> declarations;
    do {
        std::string name = currentTok.text.str();
        consume(Token::identifier);
        
        std::unique_ptr<ASTNode> value = nullptr;
        if (currentTok.is(Token::equal)) {
            advance();
            value = parseExpression();
        }
        
        declarations.emplace_back(type, name, std::move(value));
        
    } while (currentTok.is(Token::comma) && advance());
    
    consume(Token::semi_colon);
    return std::make_unique<MultiVarDeclNode>(std::move(declarations));
}

// If Statement
std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    consume(Token::KW_if);
    consume(Token::l_paren);
    auto condition = parseExpression();
    consume(Token::r_paren);
    
    auto thenBlock = parseBlock();
    
    std::unique_ptr<ASTNode> elseBlock = nullptr;
    if (currentTok.is(Token::KW_else)) {
        advance();
        elseBlock = currentTok.is(Token::KW_if) ? parseIfStatement() : parseBlock();
    }
    
    return std::make_unique<IfElseNode>(
        std::move(condition),
        std::move(thenBlock),
        std::move(elseBlock)
    );
}

// For Loop
std::unique_ptr<ASTNode> Parser::parseForLoop() {
    consume(Token::KW_for);
    consume(Token::l_paren);
    
    // Initialization
    std::unique_ptr<ASTNode> init = nullptr;
    if (!currentTok.is(Token::semi_colon)) {
        init = parseVarDeclOrExpr();
    }
    consume(Token::semi_colon);
    
    // Condition
    std::unique_ptr<ASTNode> cond = nullptr;
    if (!currentTok.is(Token::semi_colon)) {
        cond = parseExpression();
    }
    consume(Token::semi_colon);
    
    // Update
    std::unique_ptr<ASTNode> update = nullptr;
    if (!currentTok.is(Token::r_paren)) {
        update = parseExpression();
    }
    consume(Token::r_paren);
    
    auto body = parseBlock();
    
    return std::make_unique<ForLoopNode>(
        std::move(init),
        std::move(cond),
        std::move(update),
        std::move(body)
    );
}

// While Loop
std::unique_ptr<ASTNode> Parser::parseWhileLoop() {
    consume(Token::KW_while);
    consume(Token::l_paren);
    auto condition = parseExpression();
    consume(Token::r_paren);
    
    auto body = parseBlock();
    
    return std::make_unique<WhileLoopNode>(
        std::move(condition),
        std::move(body)
    );
}

// Print Statement
std::unique_ptr<ASTNode> Parser::parsePrintStatement() {
    consume(Token::KW_print);
    consume(Token::l_paren);
    auto expr = parseExpression();
    consume(Token::r_paren);
    consume(Token::semi_colon);
    return std::make_unique<PrintNode>(std::move(expr));
}

// Block Statement
std::unique_ptr<ASTNode> Parser::parseBlock() {
    consume(Token::l_brace);
    auto block = std::make_unique<BlockNode>();
    
    while (!currentTok.is(Token::r_brace) && !currentTok.is(Token::eof)) {
        block->statements.push_back(parseStatement());
    }
    
    consume(Token::r_brace);
    return block;
}

// Expression Parsing
std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseAssignment();
}

std::unique_ptr<ASTNode> Parser::parseAssignment() {
    auto left = parseLogicalOr();
    
    if (currentTok.isOneOf(Token::equal, Token::plus_equal, Token::minus_equal, 
                          Token::star_equal, Token::slash_equal, Token::mod_equal)) {
        Token op = currentTok;
        advance();
        auto right = parseAssignment();
        return std::make_unique<AssignmentNode>(std::move(left), op, std::move(right));
    }
    
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (currentTok.is(Token::or_op)) {
        advance();
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryOpNode>(BinaryOp::LogicalOr, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (currentTok.is(Token::and_op)) {
        advance();
        auto right = parseEquality();
        left = std::make_unique<BinaryOpNode>(BinaryOp::LogicalAnd, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseEquality() {
    auto left = parseComparison();
    while (currentTok.isOneOf(Token::equal_equal, Token::not_equal)) {
        BinaryOp op = currentTok.is(Token::equal_equal) ? BinaryOp::Equal : BinaryOp::NotEqual;
        advance();
        auto right = parseComparison();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseComparison() {
    auto left = parseAddSub();
    while (currentTok.isOneOf(Token::less, Token::less_equal, Token::greater, Token::greater_equal)) {
        BinaryOp op;
        switch (currentTok.kind) {
            case Token::less: op = BinaryOp::Less; break;
            case Token::less_equal: op = BinaryOp::LessEqual; break;
            case Token::greater: op = BinaryOp::Greater; break;
            case Token::greater_equal: op = BinaryOp::GreaterEqual; break;
            default: throw std::runtime_error("Invalid comparison operator");
        }
        advance();
        auto right = parseAddSub();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseAddSub() {
    auto left = parseMulDiv();
    while (currentTok.isOneOf(Token::plus, Token::minus)) {
        BinaryOp op = currentTok.is(Token::plus) ? BinaryOp::Add : BinaryOp::Subtract;
        advance();
        auto right = parseMulDiv();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseMulDiv() {
    auto left = parseUnary();
    while (currentTok.isOneOf(Token::star, Token::slash, Token::mod)) {
        BinaryOp op;
        switch (currentTok.kind) {
            case Token::star: op = BinaryOp::Multiply; break;
            case Token::slash: op = BinaryOp::Divide; break;
            case Token::mod: op = BinaryOp::Modulus; break;
            default: throw std::runtime_error("Invalid multiplicative operator");
        }
        advance();
        auto right = parseUnary();
        left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseUnary() {
    if (currentTok.isOneOf(Token::plus, Token::minus, Token::not_op)) {
        UnaryOp op;
        switch (currentTok.kind) {
            case Token::plus: op = UnaryOp::Plus; break;
            case Token::minus: op = UnaryOp::Minus; break;
            case Token::not_op: op = UnaryOp::LogicalNot; break;
            default: throw std::runtime_error("Invalid unary operator");
        }
        advance();
        auto operand = parseUnary();
        return std::make_unique<UnaryOpNode>(op, std::move(operand));
    }
    return parsePrimary();
}

std::unique_ptr<ASTNode> Parser::parsePrimary() {
    switch (currentTok.kind) {
        case Token::number:
        case Token::float_literal: {
            auto value = currentTok.text.str();
            advance();
            return std::make_unique<LiteralNode>(value);
        }
        
        case Token::string_literal: {
            auto value = currentTok.text.str();
            advance();
            return std::make_unique<StringLiteralNode>(value);
        }
        
        case Token::KW_true:
        case Token::KW_false: {
            bool value = currentTok.is(Token::KW_true);
            advance();
            return std::make_unique<BooleanLiteralNode>(value);
        }
        
        case Token::identifier: {
            std::string name = currentTok.text.str();
            advance();

            if (currentTok.is(Token::plus_plus) || currentTok.is(Token::minus_minus)) {
                Token opTok = currentTok;
                advance();  // consume ++ or --
                return std::make_unique<UnaryOpNode>(
                    opTok.is(Token::plus_plus)
                        ? UnaryOp::INCREMENT
                        : UnaryOp::DECREMENT,
                    std::make_unique<VariableNode>(name)
                );
            }

            // Function call or array access
            if (currentTok.is(Token::l_paren)) {
                return parseFunctionCall(name);
            } else if (currentTok.is(Token::l_bracket)) {
                return parseArrayAccess(name);
            }
            
            return std::make_unique<VariableNode>(name);
        }
        
        case Token::l_paren: {
            advance();
            auto expr = parseExpression();
            consume(Token::r_paren);
            return expr;
        }
        
        case Token::l_bracket:
            return parseArrayLiteral();
            
        default:
            throw std::runtime_error("Unexpected primary expression");
    }
}

std::unique_ptr<ASTNode> Parser::parseArrayLiteral() {
    consume(Token::l_bracket);
    std::vector<std::unique_ptr<ASTNode>> elements;
    
    if (!currentTok.is(Token::r_bracket)) {
        do {
            elements.push_back(parseExpression());
        } while (currentTok.is(Token::comma) && advance());
    }
    
    consume(Token::r_bracket);
    return std::make_unique<ArrayLiteralNode>(std::move(elements));
}

std::unique_ptr<ASTNode> Parser::parseArrayAccess(const std::string& name) {
    consume(Token::l_bracket);
    auto index = parseExpression();
    consume(Token::r_bracket);
    return std::make_unique<ArrayAccessNode>(name, std::move(index));
}

std::unique_ptr<ASTNode> Parser::parseFunctionCall(const std::string& name) {
    consume(Token::l_paren);
    std::vector<std::unique_ptr<ASTNode>> args;
    
    if (!currentTok.is(Token::r_paren)) {
        do {
            args.push_back(parseExpression());
        } while (currentTok.is(Token::comma) && advance());
    }
    
    consume(Token::r_paren);
    return std::make_unique<FunctionCallNode>(name, std::move(args));
}

// Helper Functions
VarType Parser::tokenToVarType(Token::TokenKind kind) {
    switch (kind) {
        case Token::KW_int: return VarType::Int;
        case Token::KW_bool: return VarType::Bool;
        case Token::KW_float: return VarType::Float;
        case Token::KW_char: return VarType::Char;
        case Token::KW_string: return VarType::String;
        case Token::KW_array: return VarType::Array;
        default: throw std::runtime_error("Invalid variable type");
    }
}

std::string Parser::tokenToString(Token::TokenKind kind) {
    // Implementation depends on your token names
    return "Token";
}
