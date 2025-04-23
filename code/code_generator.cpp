#include "codegen.h"
#include "AST.h"
#include "semantic.h"
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

CodeGen::CodeGen() : context(std::make_unique<LLVMContext>()),
                     module(std::make_unique<Module>("main", *context)),
                     builder(std::make_unique<IRBuilder<>>(*context)) {
    
    FunctionType* mainType = FunctionType::get(Type::getInt32Ty(*context), false);
    mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", *module);
    BasicBlock* entry = BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entry);

    printfFunc = Function::Create(
        FunctionType::get(Type::getInt32Ty(*context), {Type::getInt8PtrTy(*context)}, true),
        Function::ExternalLinkage, "printf", module.get()
    );

    Function::Create(
        FunctionType::get(Type::getInt8PtrTy(*context), {Type::getInt32Ty(*context)}, false),
        Function::ExternalLinkage, "malloc", module.get()
    );

    Function::Create(
        FunctionType::get(Type::getVoidTy(*context), 
        {Type::getInt8PtrTy(*context), Type::getInt8PtrTy(*context), Type::getInt32Ty(*context)}, 
        false),
        Function::ExternalLinkage, "memcpy", module.get()
    );
}

void CodeGen::generate(ProgramNode& ast) {
    for (auto& stmt : ast.statements) {
        generateStatement(stmt.get());
    }
    
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(ConstantInt::get(Type::getInt32Ty(*context), 0));
    }
    
    std::string error;
    raw_string_ostream os(error);
    if (verifyModule(*module, &os)) {
        throw std::runtime_error("Invalid IR: " + error);
    }
}

void CodeGen::generateStatement(ASTNode* node) {
    if (auto multiVarDecl = dynamic_cast<MultiVarDeclNode*>(node)) {
        for (auto& decl : multiVarDecl->declarations) {
            generateVarDecl(decl.get());
        }
    } else if (auto assign = dynamic_cast<AssignNode*>(node)) {
        generateAssign(assign);
    } else if (auto ifElse = dynamic_cast<IfElseNode*>(node)) {
        generateIfElse(ifElse);
    } else if (auto loop = dynamic_cast<ForLoopNode*>(node)) {
        generateForLoop(loop);
    } else if (auto print = dynamic_cast<PrintNode*>(node)) {
        generatePrint(print);
    } else if (auto block = dynamic_cast<BlockNode*>(node)) {
        generateBlock(block);
    } else if (auto tryCatch = dynamic_cast<TryCatchNode*>(node)) {
        generateTryCatch(tryCatch);
    } else if (auto match = dynamic_cast<MatchNode*>(node)) {
        generateMatch(match);
    } else if (auto unary = dynamic_cast<UnaryOpNode*>(node)) {
        generateUnaryOp(unary);
    } else if (auto concat = dynamic_cast<ConcatNode*>(node)) {
        generateConcat(concat);
    }
}

void CodeGen::generateVarDecl(VarDeclNode* node) {
    Type* type = nullptr;
    switch(node->type) {
        case VarType::INT: type = Type::getInt32Ty(*context); break;
        case VarType::FLOAT: type = Type::getFloatTy(*context); break;
        case VarType::BOOL: type = Type::getInt1Ty(*context); break;
        case VarType::STRING: type = Type::getInt8PtrTy(*context); break;
        case VarType::ARRAY: 
            type = PointerType::get(Type::getInt32Ty(*context), 0);
            break;
        default: throw std::runtime_error("Unknown type");
    }
    
    AllocaInst* alloca = builder->CreateAlloca(type, nullptr, node->name);
    symbols[node->name] = alloca;
    
    if (node->value) {
        Value* val = generateValue(node->value.get(), type);
        builder->CreateStore(val, alloca);
        
        if (node->type == VarType::ARRAY) {
            if (auto arrLit = dynamic_cast<ArrayNode*>(node->value.get())) {
                arraySizes[node->name] = arrLit->elements.size();
            }
        }
    }
}

Value* CodeGen::generateValue(ASTNode* node, Type* expectedType) {
    if (auto binOp = dynamic_cast<BinaryOpNode*>(node)) {
        return generateBinaryOp(binOp, expectedType);
    } else if (auto varRef = dynamic_cast<VarRefNode*>(node)) {
        if (!symbols.count(varRef->name)) {
            throw std::runtime_error("Undefined variable: " + varRef->name);
        }
        return builder->CreateLoad(symbols[varRef->name]->getAllocatedType(), symbols[varRef->name]);
    } else if (auto intLit = dynamic_cast<LiteralNode<int>*>(node)) {
        return ConstantInt::get(Type::getInt32Ty(*context), intLit->value);
    } else if (auto floatLit = dynamic_cast<LiteralNode<float>*>(node)) {
        return ConstantFP::get(Type::getFloatTy(*context), floatLit->value);
    } else if (auto strLit = dynamic_cast<LiteralNode<std::string>*>(node)) {
        return builder->CreateGlobalStringPtr(strLit->value);
    } else if (auto arr = dynamic_cast<ArrayNode*>(node)) {
        return generateArray(arr, expectedType);
    } else if (auto access = dynamic_cast<ArrayAccessNode*>(node)) {
        return generateArrayAccess(access);
    }
    throw std::runtime_error("Unsupported node type");
}

Value* CodeGen::generateBinaryOp(BinaryOpNode* node, Type* expectedType) {
    Value* L = generateValue(node->left.get(), expectedType);
    Value* R = generateValue(node->right.get(), expectedType);
    
    switch(node->op) {
        case BinaryOp::ADD:
            if (L->getType()->isFloatTy() || R->getType()->isFloatTy()) {
                return builder->CreateFAdd(L, R);
            } else {
                return builder->CreateAdd(L, R);
            }
        case BinaryOp::SUBTRACT:
            if (L->getType()->isFloatTy() || R->getType()->isFloatTy()) {
                return builder->CreateFSub(L, R);
            } else {
                return builder->CreateSub(L, R);
            }
        case BinaryOp::MULTIPLY:
            if (L->getType()->isFloatTy() || R->getType()->isFloatTy()) {
                return builder->CreateFMul(L, R);
            } else {
                return builder->CreateMul(L, R);
            }
        case BinaryOp::DIVIDE:
            if (L->getType()->isFloatTy() || R->getType()->isFloatTy()) {
                return builder->CreateFDiv(L, R);
            } else {
                return builder->CreateSDiv(L, R);
            }
        case BinaryOp::INDEX:
            return generateArrayIndex(L, R);
        case BinaryOp::CONCAT:
            return generateStringConcat(L, R);
        default:
            throw std::runtime_error("Unsupported binary op");
    }
}

Value* CodeGen::generateArrayIndex(Value* arrayPtr, Value* index) {
    Value* gep = builder->CreateGEP(
        Type::getInt32Ty(*context), 
        arrayPtr, 
        {index}
    );
    return builder->CreateLoad(Type::getInt32Ty(*context), gep);
}

Value* CodeGen::generateStringConcat(Value* L, Value* R) {
    Function* strlenFunc = module->getFunction("strlen");
    Function* mallocFunc = module->getFunction("malloc");
    Function* memcpyFunc = module->getFunction("memcpy");
    
    Value* lenL = builder->CreateCall(strlenFunc, {L});
    Value* lenR = builder->CreateCall(strlenFunc, {R});
    Value* totalLen = builder->CreateAdd(lenL, lenR);
    totalLen = builder->CreateAdd(totalLen, ConstantInt::get(Type::getInt32Ty(*context), 1));
    
    Value* buffer = builder->CreateCall(mallocFunc, {totalLen});
    builder->CreateCall(memcpyFunc, {buffer, L, lenL});
    Value* dest = builder->CreateGEP(Type::getInt8Ty(*context), buffer, lenL);
    builder->CreateCall(memcpyFunc, {dest, R, lenR});
    
    return buffer;
}

void CodeGen::generateIfElse(IfElseNode* node) {
    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* thenBB = BasicBlock::Create(*context, "then", func);
    BasicBlock* elseBB = BasicBlock::Create(*context, "else");
    BasicBlock* mergeBB = BasicBlock::Create(*context, "ifcont");
    
    Value* cond = generateValue(node->condition.get(), Type::getInt1Ty(*context));
    builder->CreateCondBr(cond, thenBB, elseBB);
    
    builder->SetInsertPoint(thenBB);
    generateStatement(node->thenBlock.get());
    builder->CreateBr(mergeBB);
    
    func->getBasicBlockList().push_back(elseBB);
    builder->SetInsertPoint(elseBB);
    if (node->elseBlock) {
        generateStatement(node->elseBlock.get());
    }
    builder->CreateBr(mergeBB);
    
    func->getBasicBlockList().push_back(mergeBB);
    builder->SetInsertPoint(mergeBB);
}

void CodeGen::generateForLoop(ForLoopNode* node) {
    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* loopStart = BasicBlock::Create(*context, "loop.start", func);
    BasicBlock* loopBody = BasicBlock::Create(*context, "loop.body");
    BasicBlock* loopEnd = BasicBlock::Create(*context, "loop.end");
    
    if (node->init) generateStatement(node->init.get());
    builder->CreateBr(loopStart);
    
    builder->SetInsertPoint(loopStart);
    Value* cond = node->condition ? 
        generateValue(node->condition.get(), Type::getInt1Ty(*context)) : 
        ConstantInt::getTrue(*context);
    builder->CreateCondBr(cond, loopBody, loopEnd);
    
    func->getBasicBlockList().push_back(loopBody);
    builder->SetInsertPoint(loopBody);
    generateStatement(node->body.get());
    if (node->update) generateStatement(node->update.get());
    builder->CreateBr(loopStart);
    
    func->getBasicBlockList().push_back(loopEnd);
    builder->SetInsertPoint(loopEnd);
}

void CodeGen::generatePrint(PrintNode* node) {
    Value* value = generateValue(node->expr.get(), nullptr);
    Type* ty = value->getType();
    
    Constant* format = nullptr;
    if (ty->isIntegerTy(32)) {
        format = builder->CreateGlobalStringPtr("%d\n");
    } else if (ty->isFloatTy()) {
        format = builder->CreateGlobalStringPtr("%f\n");
    } else if (ty->isPointerTy()) {
        format = builder->CreateGlobalStringPtr("%s\n");
    }
    
    builder->CreateCall(printfFunc, {format, value});
}

void CodeGen::generateTryCatch(TryCatchNode* node) {
    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* tryBB = BasicBlock::Create(*context, "try", func);
    BasicBlock* catchBB = BasicBlock::Create(*context, "catch");
    BasicBlock* contBB = BasicBlock::Create(*context, "try.cont");
    
    LandingPadInst* lp = builder->CreateLandingPad(
        StructType::get(Type::getInt8PtrTy(*context), Type::getInt32Ty(*context)), 0);
    lp->addClause(ConstantPointerNull::get(Type::getInt8PtrTy(*context)));
    
    builder->CreateBr(tryBB);
    
    builder->SetInsertPoint(tryBB);
    generateStatement(node->tryBlock.get());
    builder->CreateBr(contBB);
    
    func->getBasicBlockList().push_back(catchBB);
    builder->SetInsertPoint(catchBB);
    if (!node->errorVar.empty()) {
        AllocaInst* alloca = builder->CreateAlloca(Type::getInt8PtrTy(*context), nullptr, node->errorVar);
        builder->CreateStore(lp->getOperand(0), alloca);
    }
    generateStatement(node->catchBlock.get());
    builder->CreateBr(contBB);
    
    func->getBasicBlockList().push_back(contBB);
    builder->SetInsertPoint(contBB);
}

void CodeGen::generateMatch(MatchNode* node) {
    Value* expr = generateValue(node->expr.get(), nullptr);
    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* endBB = BasicBlock::Create(*context, "match.end");
    
    std::vector<BasicBlock*> caseBBs;
    for (size_t i = 0; i < node->cases.size(); ++i) {
        caseBBs.push_back(BasicBlock::Create(*context, "case." + std::to_string(i), func));
    }
    
    for (size_t i = 0; i < node->cases.size(); ++i) {
        builder->SetInsertPoint(caseBBs[i]);
        if (node->cases[i]->value) {
            Value* caseVal = generateValue(node->cases[i]->value.get(), expr->getType());
            Value* cmp = builder->CreateICmpEQ(expr, caseVal);
            BasicBlock* nextBB = (i < node->cases.size()-1) ? caseBBs[i+1] : endBB;
            builder->CreateCondBr(cmp, caseBBs[i], nextBB);
        } else {
            generateStatement(node->cases[i]->body.get());
            builder->CreateBr(endBB);
        }
    }
    
    func->getBasicBlockList().push_back(endBB);
    builder->SetInsertPoint(endBB);
}



// ... ادامه از قسمت قبلی

Value* CodeGen::generateArray(ArrayNode* node, Type* expectedType) {
    ArrayType* arrType = ArrayType::get(Type::getInt32Ty(*context), node->elements.size());
    Value* arrayPtr = builder->CreateAlloca(arrType);
    
    for (size_t i = 0; i < node->elements.size(); ++i) {
        Value* idx[] = {
            ConstantInt::get(Type::getInt32Ty(*context), 0),
            ConstantInt::get(Type::getInt32Ty(*context), i)
        };
        Value* elemPtr = builder->CreateInBoundsGEP(arrType, arrayPtr, idx);
        Value* val = generateValue(node->elements[i].get(), Type::getInt32Ty(*context));
        builder->CreateStore(val, elemPtr);
    }
    
    return builder->CreateBitCast(arrayPtr, expectedType);
}

Value* CodeGen::generateArrayAccess(ArrayAccessNode* node) {
    Value* arrayPtr = symbols[node->arrayName];
    Value* index = generateValue(node->index.get(), Type::getInt32Ty(*context));
    
    // Runtime bounds checking
    if (arraySizes.count(node->arrayName)) {
        Value* size = ConstantInt::get(Type::getInt32Ty(*context), arraySizes[node->arrayName]);
        Value* cond = builder->CreateICmpSGE(index, size);
        
        BasicBlock* errorBB = BasicBlock::Create(*context, "bounds_error", builder->GetInsertBlock()->getParent());
        BasicBlock* validBB = BasicBlock::Create(*context, "valid_index");
        
        builder->CreateCondBr(cond, errorBB, validBB);
        
        builder->SetInsertPoint(errorBB);
        Function* throwFunc = Function::Create(
            FunctionType::get(Type::getVoidTy(*context), {Type::getInt8PtrTy(*context)}, false),
            Function::ExternalLinkage, "throw_exception", module.get()
        );
        Value* msg = builder->CreateGlobalStringPtr("Array index out of bounds!");
        builder->CreateCall(throwFunc, {msg});
        builder->CreateUnreachable();
        
        builder->GetInsertBlock()->getParent()->getBasicBlockList().push_back(validBB);
        builder->SetInsertPoint(validBB);
    }
    
    return generateArrayIndex(arrayPtr, index);
}

Value* CodeGen::generateUnaryOp(UnaryOpNode* node) {
    Value* operand = generateValue(node->operand.get(), nullptr);
    
    switch(node->op) {
        case UnaryOp::MINUS:
            if (operand->getType()->isFloatTy()) {
                return builder->CreateFNeg(operand);
            } else {
                return builder->CreateNeg(operand);
            }
        case UnaryOp::INCREMENT: {
            Value* inc = ConstantInt::get(operand->getType(), 1);
            Value* newVal = builder->CreateAdd(operand, inc);
            if (auto varRef = dynamic_cast<VarRefNode*>(node->operand.get())) {
                builder->CreateStore(newVal, symbols[varRef->name]);
            }
            return operand; // Post-increment returns original value
        }
        case UnaryOp::LENGTH: {
            if (!arraySizes.count(node->operand->getName())) {
                throw std::runtime_error("Length operator on non-array type");
            }
            return ConstantInt::get(Type::getInt32Ty(*context), arraySizes[node->operand->getName()]);
        }
        default:
            throw std::runtime_error("Unsupported unary operator");
    }
}

void CodeGen::generateCompoundAssign(CompoundAssignNode* node) {
    Value* target = symbols[node->target];
    Value* current = builder->CreateLoad(target->getType()->getPointerElementType(), target);
    Value* rhs = generateValue(node->value.get(), current->getType());
    
    Value* result;
    switch(node->op) {
        case BinaryOp::ADD:
            result = current->getType()->isFloatTy() ? 
                builder->CreateFAdd(current, rhs) : 
                builder->CreateAdd(current, rhs);
            break;
        case BinaryOp::SUBTRACT:
            result = current->getType()->isFloatTy() ? 
                builder->CreateFSub(current, rhs) : 
                builder->CreateSub(current, rhs);
            break;
        default:
            throw std::runtime_error("Unsupported compound assignment");
    }
    
    builder->CreateStore(result, target);
}

void CodeGen::generateWhileLoop(WhileLoopNode* node) {
    Function* func = builder->GetInsertBlock()->getParent();
    BasicBlock* condBB = BasicBlock::Create(*context, "while.cond", func);
    BasicBlock* bodyBB = BasicBlock::Create(*context, "while.body");
    BasicBlock* endBB = BasicBlock::Create(*context, "while.end");
    
    builder->CreateBr(condBB);
    
    // Condition block
    builder->SetInsertPoint(condBB);
    Value* cond = generateValue(node->condition.get(), Type::getInt1Ty(*context));
    builder->CreateCondBr(cond, bodyBB, endBB);
    
    // Body block
    func->getBasicBlockList().push_back(bodyBB);
    builder->SetInsertPoint(bodyBB);
    generateStatement(node->body.get());
    builder->CreateBr(condBB);  // Loop back
    
    // End block
    func->getBasicBlockList().push_back(endBB);
    builder->SetInsertPoint(endBB);
}

Value* CodeGen::generateTernaryExpr(TernaryExprNode* node) {
    Value* cond = generateValue(node->condition.get(), Type::getInt1Ty(*context));
    Function* func = builder->GetInsertBlock()->getParent();
    
    BasicBlock* trueBB = BasicBlock::Create(*context, "ternary.true", func);
    BasicBlock* falseBB = BasicBlock::Create(*context, "ternary.false");
    BasicBlock* mergeBB = BasicBlock::Create(*context, "ternary.merge");
    
    builder->CreateCondBr(cond, trueBB, falseBB);
    
    // True branch
    builder->SetInsertPoint(trueBB);
    Value* trueVal = generateValue(node->trueExpr.get(), nullptr);
    builder->CreateBr(mergeBB);
    
    // False branch
    func->getBasicBlockList().push_back(falseBB);
    builder->SetInsertPoint(falseBB);
    Value* falseVal = generateValue(node->falseExpr.get(), nullptr);
    builder->CreateBr(mergeBB);
    
    // Merge
    func->getBasicBlockList().push_back(mergeBB);
    builder->SetInsertPoint(mergeBB);
    PHINode* phi = builder->CreatePHI(trueVal->getType(), 2);
    phi->addIncoming(trueVal, trueBB);
    phi->addIncoming(falseVal, falseBB);
    
    return phi;
}

void CodeGen::generateTypeConversion(TypeConversionNode* node) {
    Value* val = generateValue(node->expr.get(), nullptr);
    Type* targetType = getLLVMType(node->targetType);
    
    if (val->getType() == targetType) return val;
    
    if (targetType->isFloatTy() && val->getType()->isIntegerTy()) {
        return builder->CreateSIToFP(val, targetType);
    }
    if (targetType->isIntegerTy() && val->getType()->isFloatTy()) {
        return builder->CreateFPToSI(val, targetType);
    }
    if (targetType->isPointerTy() && val->getType()->isIntegerTy()) {
        return builder->CreateIntToPtr(val, targetType);
    }
    
    throw std::runtime_error("Invalid type conversion");
}

Value* CodeGen::generateBuiltInCall(BuiltInCallNode* node) {
    if (node->funcName == "pow") {
        Value* base = generateValue(node->args[0].get(), Type::getDoubleTy(*context));
        Value* exp = generateValue(node->args[1].get(), Type::getDoubleTy(*context));
        return builder->CreateCall(
            Intrinsic::getDeclaration(module.get(), Intrinsic::pow, {Type::getDoubleTy(*context)}),
            {base, exp}
        );
    }
    if (node->funcName == "abs") {
        Value* val = generateValue(node->args[0].get(), nullptr);
        if (val->getType()->isFloatTy()) {
            return builder->CreateCall(
                Intrinsic::getDeclaration(module.get(), Intrinsic::fabs, {val->getType()}),
                {val}
            );
        } else {
            Value* zero = ConstantInt::get(val->getType(), 0);
            Value* neg = builder->CreateNeg(val);
            return builder->CreateSelect(
                builder->CreateICmpSGT(val, zero),
                val,
                neg
            );
        }
    }
    throw std::runtime_error("Unknown built-in function");
}

void CodeGen::generateMemoryManagement() {
    // Register destructors for stack-allocated arrays
    for (auto& [name, alloca] : symbols) {
        if (alloca->getAllocatedType()->isPointerTy()) {
            Function* freeFunc = Function::Create(
                FunctionType::get(Type::getVoidTy(*context), {Type::getInt8PtrTy(*context)}, false),
                Function::ExternalLinkage, "free", module.get()
            );
            
            BasicBlock* currentBB = builder->GetInsertBlock();
            BasicBlock* freeBB = BasicBlock::Create(*context, "free." + name, currentBB->getParent());
            
            builder->SetInsertPoint(freeBB);
            Value* casted = builder->CreateBitCast(alloca, Type::getInt8PtrTy(*context));
            builder->CreateCall(freeFunc, {casted});
            builder->CreateBr(currentBB);
            
            currentBB->getParent()->getBasicBlockList().push_back(freeBB);
        }
    }
}

void CodeGen::optimizeIR() {
    legacy::FunctionPassManager FPM(module.get());
    FPM.add(createPromoteMemoryToRegisterPass());
    FPM.add(createInstructionCombiningPass());
    FPM.add(createReassociatePass());
    FPM.add(createGVNPass());
    FPM.add(createCFGSimplificationPass());
    
    FPM.doInitialization();
    for (Function &F : *module) {
        FPM.run(F);
    }
}



void CodeGen::dump() const {
    module->print(llvm::outs(), nullptr);
}

extern "C" void throw_exception(const char* msg) {
    throw std::runtime_error(msg);
}
