#ifndef AST_H
#define AST_H

#include <memory>
#include <vector>
#include <string>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>

enum class VarType { 
    INT, BOOL, FLOAT, CHAR, 
    STRING, ARRAY, ERROR, NEUTRAL 
};

enum class BinaryOp { 
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MOD,
    EQUAL, NOT_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    AND, OR, INDEX, CONCAT, POW, 
    ARRAY_ADD, ARRAY_SUBTRACT, ARRAY_MULTIPLY, ARRAY_DIVIDE 
};

enum class UnaryOp { 
    INCREMENT, DECREMENT, 
    LENGTH, MIN, MAX, ABS 
};

enum class LoopType { FOR, FOREACH, WHILE };

enum class StmtType { 
    IF, ELSE_IF, ELSE, 
    FOR, WHILE, TRY_CATCH, 
    VAR_DECL, ASSIGN, PRINT, 
    BLOCK, MATCH 
};

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void print(llvm::raw_ostream &os, int indent = 0) const = 0;
};

// barname  - majmoo e ii az gozaare ha
class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;
    
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Var decleration
class VarDeclNode : public ASTNode {
public:
    VarType type;
    std::string name;
    std::unique_ptr<ASTNode> value;
    
    VarDeclNode(VarType t, std::string n, std::unique_ptr<ASTNode> v)
        : type(t), name(n), value(std::move(v)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// tarif chand moteghayyer
class MultiVarDeclNode : public ASTNode {
public:
    std::vector<std::unique_ptr<VarDeclNode>> declarations;
    
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Assignment
class AssignNode : public ASTNode {
public:
    std::string target;
    BinaryOp op;
    std::unique_ptr<ASTNode> value;
    
    AssignNode(std::string t, BinaryOp o, std::unique_ptr<ASTNode> v)
        : target(t), op(o), value(std::move(v)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Refrence to Variable
class VarRefNode : public ASTNode {
public:
    std::string name;
    
    VarRefNode(std::string n) : name(n) {}
    
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Literals
template<typename T>
class LiteralNode : public ASTNode {
public:
    T value;
    
    LiteralNode(T val) : value(val) {}
    
    void print(llvm::raw_ostream &os, int indent = 0) const override {
        os << value;
    }
};

using IntLiteral = LiteralNode<int>;
using FloatLiteral = LiteralNode<float>;
using BoolLiteral = LiteralNode<bool>;
using CharLiteral = LiteralNode<char>;
using StrLiteral = LiteralNode<std::string>;

// Binaray
class BinaryOpNode : public ASTNode {
public:
    BinaryOp op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    
    BinaryOpNode(BinaryOp o, std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Unary
class UnaryOpNode : public ASTNode {
public:
    UnaryOp op;
    std::unique_ptr<ASTNode> operand;
    
    UnaryOpNode(UnaryOp o, std::unique_ptr<ASTNode> opnd)
        : op(o), operand(std::move(opnd)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Code Block
class BlockNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;
    
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// if/else
class IfElseNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<BlockNode> thenBlock;
    std::unique_ptr<ASTNode> elseBlock; // Can be BlockNode or IfElseNode
    
    IfElseNode(std::unique_ptr<ASTNode> cond, 
              std::unique_ptr<BlockNode> thenBlk,
              std::unique_ptr<ASTNode> elseBlk = nullptr)
        : condition(std::move(cond)), 
          thenBlock(std::move(thenBlk)),
          elseBlock(std::move(elseBlk)) {}
          
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// for
class ForLoopNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> update;
    std::unique_ptr<BlockNode> body;
    
    ForLoopNode(std::unique_ptr<ASTNode> i, 
               std::unique_ptr<ASTNode> cond,
               std::unique_ptr<ASTNode> upd,
               std::unique_ptr<BlockNode> b)
        : init(std::move(i)), condition(std::move(cond)),
          update(std::move(upd)), body(std::move(b)) {}
          
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// foreach
class ForeachLoopNode : public ASTNode {
public:
    std::string varName;
    std::unique_ptr<ASTNode> collection;
    std::unique_ptr<BlockNode> body;
    
    ForeachLoopNode(std::string var, 
                   std::unique_ptr<ASTNode> coll,
                   std::unique_ptr<BlockNode> b)
        : varName(var), collection(std::move(coll)), body(std::move(b)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// print
class PrintNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    
    PrintNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// araye
class ArrayNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> elements;
    
    ArrayNode(std::vector<std::unique_ptr<ASTNode>> elems)
        : elements(std::move(elems)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// nth khoone Arraye  
class ArrayAccessNode : public ASTNode {
public:
    std::string arrayName;
    std::unique_ptr<ASTNode> index;
    
    ArrayAccessNode(std::string name, std::unique_ptr<ASTNode> idx)
        : arrayName(name), index(std::move(idx)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Concat
class ConcatNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    
    ConcatNode(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r)
        : left(std::move(l)), right(std::move(r)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

class PowNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> base;
    std::unique_ptr<ASTNode> exponent;
    
    PowNode(std::unique_ptr<ASTNode> b, std::unique_ptr<ASTNode> e)
        : base(std::move(b)), exponent(std::move(e)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// Error Control
class TryCatchNode : public ASTNode {
public:
    std::unique_ptr<BlockNode> tryBlock;
    std::unique_ptr<BlockNode> catchBlock;
    std::string errorVar;
    
    TryCatchNode(std::unique_ptr<BlockNode> tryBlk,
                std::unique_ptr<BlockNode> catchBlk,
                std::string errVar)
        : tryBlock(std::move(tryBlk)), 
          catchBlock(std::move(catchBlk)),
          errorVar(errVar) {}
          
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

// match pattern
class MatchNode : public ASTNode {
public:
    std::unique_ptr<ASTNode> expr;
    std::vector<std::unique_ptr<ASTNode>> cases;
    
    MatchNode(std::unique_ptr<ASTNode> e, std::vector<std::unique_ptr<ASTNode>> c)
        : expr(std::move(e)), cases(std::move(c)) {}
        
    void print(llvm::raw_ostream &os, int indent = 0) const override;
};

#endif
