#ifndef PARSER_H
#define PARSER_H
#include "error.h"
#include "parser.h"
#include "optimizer.h"
#include <string>
using namespace std;
#endif

Base *Parser::parse()
{
    llvm::SmallVector<Statement *> statements;
    bool isComment = false;
    while (!Tok.is(Token::eof))
    {
        if (isComment)
        {
            if (Tok.is(Token::uncomment))
            {
                isComment = false;
            }
            advance();
            continue;
        }

        switch (Tok.getKind())
        {
        case Token::KW_int:
        {
            llvm::SmallVector<DecStatement *> states = Parser::parseDefine(Token::KW_int);
            if (states.size() == 0)
            {
                return nullptr;
            }
            while (states.size() > 0)
            {
                statements.push_back(states.back());
                states.pop_back();
            }
            break;
        }
        case Token::KW_bool:
        {
            llvm::SmallVector<DecStatement *> states = Parser::parseDefine(Token::KW_bool);
            if (states.size() == 0)
            {
                return nullptr;
            }
            while (states.size() > 0)
            {
                statements.push_back(states.back());
                states.pop_back();
            }
            break;
        }
        case Token::identifier:
        {
            llvm::StringRef name = Tok.getText();
            Token current = Tok;
            advance();
            if (!Tok.isOneOf(Token::plus_plus, Token::minus_minus))
            {
                AssignStatement *assign = parseAssign(name);
                statements.push_back(assign);
            }
            else
            {
                AssignStatement *assign = parseUnaryExpression(current);
                statements.push_back(assign);
            }
            Parser::check_for_semicolon();
            break;
        }
        case Token::KW_print:
        {
            advance();
            if (!Tok.is(Token::l_paren))
            {
                Error::LeftParenthesisExpected();
            }
            advance();
            // token should be identifier
            if (!Tok.is(Token::identifier))
            {
                Error::VariableExpected();
            }
            Expression *variable_to_be_printed = new Expression(Tok.getText());
            advance();
            if (!Tok.is(Token::r_paren))
            {
                Error::RightParenthesisExpected();
            }
            advance();
            Parser::check_for_semicolon();
            PrintStatement *print_statement = new PrintStatement(variable_to_be_printed);
            statements.push_back(print_statement);
            break;
        }
        case Token::comment:
        {
            isComment = true;
            advance();
            break;
        }
        case Token::KW_if:
        {
            IfStatement *statement = parseIf();
            statements.push_back(statement);
            break;
        }
        case Token::KW_while:
        {
            WhileStatement *statement = parseWhile();
            statements.push_back(statement);
            break;
        }
        case Token::KW_for:
        {
            ForStatement *statement = parseFor();
            llvm::SmallVector <Statement *> unrolled = completeUnroll(statement);
            for (Statement *s : unrolled)
            {
                statements.push_back(s);
            }
            break;
        }
        }
    }
    return new Base(statements);
}

void Parser::check_for_semicolon()
{
    if (!Tok.is(Token::semi_colon))
    {
        Error::SemiColonExpected();
    }
    advance();
}

AssignStatement *Parser::parseUnaryExpression(Token &token)
{
    AssignStatement* res;
    if (Tok.is(Token::plus_plus))
    {
        advance();
        if (token.is(Token::identifier))
        {
            Expression *tok = new Expression(token.getText());
            Expression *one = new Expression(1);
            res = new AssignStatement(tok, new BinaryOp(BinaryOp::Plus, tok, one));
        }
        else
        {
            Error::VariableExpected();
        }
    }
    else if (Tok.is(Token::minus_minus))
    {
        advance();
        if (token.is(Token::identifier))
        {
            Expression *tok = new Expression(token.getText());
            Expression *one = new Expression(1);
            res = new AssignStatement(tok, new BinaryOp(BinaryOp::Minus, tok, one));
        }
        else
        {
            Error::VariableExpected();
        }
    }
    return res;
}
llvm::SmallVector<DecStatement *> Parser::parseDefine(Token::TokenKind token_kind)
{
    advance();
    llvm::SmallVector<DecStatement *> states;
    while (!Tok.is(Token::semi_colon))
    {
        llvm::StringRef name;
        Expression *value = nullptr;
        if (Tok.is(Token::identifier))
        {
            name = Tok.getText();
            advance();
        }
        else
        {
            Error::VariableExpected();
        }
        if (Tok.is(Token::equal))
        {
            advance();
            value = parseExpression();
        }
        if (Tok.is(Token::comma))
        {
            advance();
        }
        else if (Tok.is(Token::semi_colon))
        {
            // OK do nothing. The while will break itself.
        }
        else
        {
            Error::VariableExpected();
        }
        DecStatement *state;
        if(token_kind == Token::KW_int){
            state = new DecStatement(new Expression(name), value, DecStatement::DecStatementType::Number);
        }else{
            state = new DecStatement(new Expression(name), value, DecStatement::DecStatementType::Boolean);
        }
        
        states.push_back(state);
    }

    advance(); // pass semicolon
    return states;
}

Expression *Parser::parseExpression()
{
    Expression *left = parseLogicalComparison();
    while (Tok.isOneOf(Token::KW_and, Token::KW_or))
    {
        BooleanOp::Operator Op;
        switch (Tok.getKind())
        {
        case Token::KW_and:
            Op = BooleanOp::And;
            break;
        case Token::KW_or:
            Op = BooleanOp::Or;
            break;
        default:
            break;
        }
        advance();
        Expression *Right = parseLogicalComparison();
        left = new BooleanOp(Op, left, Right);
    }
    return left;
}

Expression *Parser::parseLogicalComparison()
{
    Expression *left = parseIntExpression();
    while (Tok.isOneOf(Token::equal_equal, Token::not_equal, Token::less, Token::less_equal, Token::greater, Token::greater_equal))
    {
        BooleanOp::Operator Op;
        switch (Tok.getKind())
        {
        case Token::equal_equal:
            Op = BooleanOp::Equal;
            break;
        case Token::not_equal:
            Op = BooleanOp::NotEqual;
            break;
        case Token::less:
            Op = BooleanOp::Less;
            break;
        case Token::less_equal:
            Op = BooleanOp::LessEqual;
            break;
        case Token::greater:
            Op = BooleanOp::Greater;
            break;
        case Token::greater_equal:
            Op = BooleanOp::GreaterEqual;
            break;
        default:
            break;
        }
        advance();
        Expression *Right = parseIntExpression();
        left = new BooleanOp(Op, left, Right);
    }
    return left;
}

Expression *Parser::parseIntExpression()
{
    Expression *Left = parseTerm();
    while (Tok.isOneOf(Token::plus, Token::minus))
    {
        BinaryOp::Operator Op =
            Tok.is(Token::plus) ? BinaryOp::Plus : BinaryOp::Minus;
        advance();
        Expression *Right = parseTerm();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expression *Parser::parseTerm()
{
    Expression *Left = parseSign();
    while (Tok.isOneOf(Token::star, Token::slash, Token::mod))
    {
        BinaryOp::Operator Op =
            Tok.is(Token::star) ? BinaryOp::Mul : Tok.is(Token::slash) ? BinaryOp::Div
                                                                       : BinaryOp::Mod;
        advance();
        Expression *Right = parseSign();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expression *Parser::parseSign()
{
    if (Tok.is(Token::minus))
    {
        advance();
        return new BinaryOp(BinaryOp::Mul, new Expression(-1), parsePower());
    }
    else if (Tok.is(Token::plus))
    {
        advance();
        return parsePower();
    }
    return parsePower();
}

Expression *Parser::parsePower()
{
    Expression *Left = parseFactor();
    while (Tok.is(Token::power))
    {
        BinaryOp::Operator Op =
            BinaryOp::Pow;
        advance();
        Expression *Right = parseFactor();
        Left = new BinaryOp(Op, Left, Right);
    }
    return Left;
}

Expression *Parser::parseFactor()
{
    Expression *Res = nullptr;
    switch (Tok.getKind())
    {
    case Token::number:
    {
        int number;
        Tok.getText().getAsInteger(10, number);
        Res = new Expression(number);
        advance();
        break;
    }
    case Token::identifier:
    {
        Res = new Expression(Tok.getText());
        advance();
        break;
    }
    case Token::l_paren:
    {
        advance();
        Res = parseExpression();
        if (!consume(Token::r_paren))
            break;
    }
    case Token::KW_true:
    {
        Res = new Expression(true);
        advance();
        break;
    }
    case Token::KW_false:
    {
        Res = new Expression(false);
        advance();
        break;
    }
    default: // error handling
    {
        Error::NumberVariableExpected();
    }
    }
    return Res;
}

AssignStatement *Parser::parseAssign(llvm::StringRef name)
{
    Expression *value = nullptr;
    if (Tok.is(Token::equal))
    {
        advance();
        value = parseExpression();
    }else if(Tok.isOneOf(Token::plus_equal, Token::minus_equal, Token::star_equal, Token::slash_equal, Token::mod_equal)){
        // storing token
        Token current_op = Tok;
        advance();
        value = parseExpression();
        if(current_op.is(Token::plus_equal)){
            value = new BinaryOp(BinaryOp::Plus, new Expression(name), value);
        }else if(current_op.is(Token::minus_equal)){
            value = new BinaryOp(BinaryOp::Minus, new Expression(name), value);
        }else if(current_op.is(Token::star_equal)){
            value = new BinaryOp(BinaryOp::Mul, new Expression(name), value);
        }else if(current_op.is(Token::slash_equal)){
            value = new BinaryOp(BinaryOp::Div, new Expression(name), value);
        }else if(current_op.is(Token::mod_equal)){
            value = new BinaryOp(BinaryOp::Mod, new Expression(name), value);
        }
    }else{
        Error::EqualExpected();
    }
    
    return new AssignStatement(new Expression(name), value);
}

Base *Parser::parseStatement()
{
    llvm::SmallVector<Statement *> statements;
    bool isComment = false;
    while (!Tok.is(Token::r_brace) && !Tok.is(Token::eof))
    {
        if (isComment)
        {
            if (Tok.is(Token::uncomment))
            {
                isComment = false;
            }
            advance();
            continue;
        }

        switch (Tok.getKind())
        {
        case Token::identifier:
        {
            llvm::StringRef name = Tok.getText();
            Token current = Tok;
            advance();
            if (!Tok.isOneOf(Token::plus_plus, Token::minus_minus))
            {
                AssignStatement *assign = parseAssign(name);
                statements.push_back(assign);
            }
            else
            {
                AssignStatement *assign = parseUnaryExpression(current);
                statements.push_back(assign);
            }
            Parser::check_for_semicolon();
            break;
        }
        case Token::KW_print:
        {
            advance();
            if (!Tok.is(Token::l_paren))
            {
                Error::LeftParenthesisExpected();
            }
            advance();
            // token should be identifier
            if (!Tok.is(Token::identifier))
            {
                Error::VariableExpected();
            }
            Expression *tok = new Expression(Tok.getText());
            advance();
            if (!Tok.is(Token::r_paren))
            {
                Error::RightParenthesisExpected();
            }
            advance();
            Parser::check_for_semicolon();
            PrintStatement *print_statement = new PrintStatement(tok);
            statements.push_back(print_statement);
            break;
        }
        case Token::comment:
        {
            isComment = true;
            advance();
            break;
        }
        case Token::KW_if:
        {
            IfStatement *statement = parseIf();
            statements.push_back(statement);
            break;
        }
        case Token::KW_while:
		{
			WhileStatement* statement = parseWhile();
			statements.push_back(statement);
			break;
		}
        case Token::KW_for:
        {
            ForStatement* statement = parseFor();
            statements.push_back(statement);
            break;
        }
        default:
        {
            Error::UnexpectedToken(Tok);
        }
        }
    }
    return new Base(statements);
}

IfStatement *Parser::parseIf()
{
    advance();
    if (!Tok.is(Token::l_paren))
    {
        Error::LeftParenthesisExpected();
    }

    advance();
    Expression *condition = parseExpression();

    if (!Tok.is(Token::r_paren))
    {
        Error::RightParenthesisExpected();
    }

    advance();
    if (!Tok.is(Token::l_brace))
    {
        Error::LeftBraceExpected();
    }
    advance();
    Base *allIfStatements = parseStatement();
    if (!Tok.is(Token::r_brace))
    {
        Error::RightBraceExpected();
    }
    advance();

    // parse else if and else statements
    llvm::SmallVector<ElseIfStatement *> elseIfStatements;
    ElseStatement *elseStatement = nullptr;
    bool hasElseIf = false;
    bool hasElse = false;

    while (Tok.is(Token::KW_else))
    {
        advance();
        if (Tok.is(Token::KW_if))
        {
            elseIfStatements.push_back(parseElseIf());
            hasElseIf = true;
        }
        else if (Tok.is(Token::l_brace))
        {
            advance();
            Base *allElseStatements = parseStatement();
            if (!Tok.is(Token::r_brace))
            {
                Error::RightBraceExpected();
            }
            advance();
            elseStatement = new ElseStatement(allElseStatements->getStatements(), Statement::StatementType::Else);
            hasElse = true;
            break;
        }
        else
        {
            Error::RightBraceExpected();
        }
    }

    return new IfStatement(condition, allIfStatements->getStatements(), elseIfStatements, elseStatement, hasElseIf, hasElse, Statement::StatementType::If);
}

ElseIfStatement *Parser::parseElseIf()
{
    advance();
    if (!Tok.is(Token::l_paren))
    {
        Error::LeftParenthesisExpected();
    }

    advance();
    Expression *condition = parseExpression();

    if (!Tok.is(Token::r_paren))
    {
        Error::RightParenthesisExpected();
    }

    advance();
    if (!Tok.is(Token::l_brace))
    {
        Error::LeftBraceExpected();
    }
    advance();
    Base *allIfStatements = parseStatement();
    if (!Tok.is(Token::r_brace))
    {
        Error::RightBraceExpected();
    }
    advance();

    return new ElseIfStatement(condition, allIfStatements->getStatements(), Statement::StatementType::ElseIf);
}

WhileStatement *Parser::parseWhile()
{
    advance();
    if (!Tok.is(Token::l_paren))
    {
        Error::LeftParenthesisExpected();
    }

    advance();
    Expression *condition = parseExpression();

    if (!Tok.is(Token::r_paren))
    {
        Error::RightParenthesisExpected();
    }

    advance();
    if (Tok.is(Token::l_brace))
    {
        advance();
        Base *allWhileStatements = parseStatement();
        if(!consume(Token::r_brace))
        {
            return new WhileStatement(condition, allWhileStatements->getStatements(), Statement::StatementType::While);
        }
        else
		{
			Error::RightBraceExpected();
		}

    }
    else{
        Error::LeftBraceExpected();
    }
    advance();
}

ForStatement *Parser::parseFor()
{
    advance();
    if (!Tok.is(Token::l_paren))                // for(i = 0;i<10;i++)
    {
        Error::LeftParenthesisExpected();
    }
    advance();                                  //i = 0;i<10;i++)
    if (!Tok.is(Token::identifier))
    {
        Error::VariableExpected();

    }
    llvm::StringRef name = Tok.getText();
    advance();                                  //= 0;i<10;i++)
    AssignStatement *assign = parseAssign(name);
    //Expression *value = nullptr;
    /*if (Tok.is(Token::equal))
    {
        advance();                              //0;i<10;i++)
        value = parseExpression();
        AssignStatement *assign = new AssignStatement(new Expression(name), value);
    }*/
    //advance();
    check_for_semicolon();
    Expression *condition = parseExpression();
    //advance();
    check_for_semicolon();
    if (!Tok.is(Token::identifier))
    {
        Error::VariableExpected();

    }
    llvm::StringRef name_up = Tok.getText();
    Token current = Tok;
    AssignStatement *assign_up = nullptr;
    advance();
    if (!Tok.isOneOf(Token::plus_plus, Token::minus_minus))
    {
        assign_up = parseAssign(name_up);
    }
    else
    {
        assign_up  = parseUnaryExpression(current);
    }
    if (!Tok.is(Token::r_paren))
    {
        Error::RightParenthesisExpected();
    }

    advance();
    if (Tok.is(Token::l_brace))
    {
        advance();
        Base *allForStatements = parseStatement();
        if(!consume(Token::r_brace))
        {
            return new ForStatement(condition, allForStatements->getStatements(),assign,assign_up, Statement::StatementType::For);
        }
        else
		{
			Error::RightBraceExpected();
		}

    }
    else{
        Error::LeftBraceExpected();
    }
    advance();


}

