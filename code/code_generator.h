#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "AST.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

class CodeGen {
public:
    void compile(ProgramNode *root, bool optimize, int unroll);
    void dump() const;

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    llvm::Function* currentFunc = nullptr;
    std::unordered_map<std::string, llvm::AllocaInst*> symbols;
    std::unordered_map<std::string, uint64_t> arraySizes;

    void declareRuntimeFunctions();
    void generateStatement(ASTNode* node);
    llvm::Value* generateValue(ASTNode* node, llvm::Type* expectedType);
    llvm::Value* generatePow(llvm::Value* base, llvm::Value* exp);

    void printArray(const std::vector<llvm::Value*>& elements);
    void printArrayVar(llvm::Value* arrayPtr, uint64_t size);
    void generateTryCatch(TryCatchNode* node);
    void generateMatch(MatchNode* node);
};

#endif
