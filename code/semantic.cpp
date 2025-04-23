#include "semantic.h"
#include "AST.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"


namespace {
    static const char *varTypeName(VarType t) {
        switch (t) {
        case VarType::INT:    return "int";
        case VarType::FLOAT:  return "float";
        case VarType::BOOL:   return "bool";
        case VarType::CHAR:   return "char";
        case VarType::STRING: return "string";
        case VarType::ARRAY:  return "array";
        default:              return "<unknown>";
        }
    }

    class DeclCheck : public ASTVisitor {
        llvm::StringMap<VarType> VarTypes;
        bool HasError = false;

        enum ErrorKind { AlreadyDefined, NotDefined, DivideByZero, TypeMismatch, InvalidOperation };
        void report(ErrorKind kind, const std::string &msg) {
            switch (kind) {
            case AlreadyDefined:   llvm::errs() << "Error: variable '" << msg << "' already defined\n"; break;
            case NotDefined:       llvm::errs() << "Error: variable '" << msg << "' not defined\n"; break;
            case DivideByZero:     llvm::errs() << "Error: division by zero!\n"; break;
            case TypeMismatch:     llvm::errs() << "Error: type mismatch for '" << msg << "'\n"; break;
            case InvalidOperation: llvm::errs() << "Error: invalid operation '" << msg << "' for type\n"; break;
            }
            HasError = true;
        }


        VarType typeOf(ASTNode *node) {
            if (!node) return VarType::ERROR;
            // Literals
            if (auto *i = dynamic_cast<LiteralNode<int>*>(node))    return VarType::INT;
            if (auto *f = dynamic_cast<LiteralNode<float>*>(node))  return VarType::FLOAT;
            if (auto *b = dynamic_cast<LiteralNode<bool>*>(node))   return VarType::BOOL;
            if (auto *c = dynamic_cast<LiteralNode<char>*>(node))   return VarType::CHAR;
            if (auto *s = dynamic_cast<LiteralNode<std::string>*>(node)) return VarType::STRING;
            // Variable reference
            if (auto *v = dynamic_cast<VarRefNode*>(node)) {
                if (!VarTypes.count(v->name)) {
                    report(NotDefined, v->name);
                    return VarType::ERROR;
                }
                return VarTypes.lookup(v->name);
            }
            // Binary operations
            if (auto *bin = dynamic_cast<BinaryOpNode*>(node)) {
                VarType lt = typeOf(bin->left.get());
                VarType rt = typeOf(bin->right.get());
                switch (bin->op) {
                case BinaryOp::ADD: case BinaryOp::SUBTRACT:
                case BinaryOp::MULTIPLY: case BinaryOp::DIVIDE: case BinaryOp::MOD:
                case BinaryOp::POW: {
                    // numeric or array-scalar or scalar-array
                    if ((lt==VarType::INT||lt==VarType::FLOAT) && (rt==VarType::INT||rt==VarType::FLOAT))
                        return (lt==VarType::FLOAT||rt==VarType::FLOAT?VarType::FLOAT:VarType::INT);
                    if (lt==VarType::ARRAY && (rt==VarType::INT||rt==VarType::FLOAT)) return VarType::ARRAY;
                    if ((lt==VarType::INT||lt==VarType::FLOAT) && rt==VarType::ARRAY) return VarType::ARRAY;
                    report(InvalidOperation, varTypeName(lt) + std::string(" op ") + varTypeName(rt));
                    return VarType::ERROR;
                }
                case BinaryOp::AND: case BinaryOp::OR:
                    if (lt==VarType::BOOL && rt==VarType::BOOL) return VarType::BOOL;
                    report(InvalidOperation, "logical"); return VarType::ERROR;
                case BinaryOp::EQUAL: case BinaryOp::NOT_EQUAL:
                case BinaryOp::LESS: case BinaryOp::LESS_EQUAL:
                case BinaryOp::GREATER: case BinaryOp::GREATER_EQUAL:
                    // comparisons produce bool
                    if ((lt==VarType::INT||lt==VarType::FLOAT) && (rt==VarType::INT||rt==VarType::FLOAT)) return VarType::BOOL;
                    if (lt==rt) return VarType::BOOL;
                    report(TypeMismatch, "compare"); return VarType::ERROR;
                case BinaryOp::CONCAT:
                    if (lt==VarType::STRING && rt==VarType::STRING) return VarType::STRING;
                    report(InvalidOperation, "concat"); return VarType::ERROR;
                case BinaryOp::INDEX:
                    if (lt==VarType::ARRAY && rt==VarType::INT) return VarType::ARRAY; // element type infer omitted
                    report(InvalidOperation, "index"); return VarType::ERROR;
                default:
                    return VarType::ERROR;
                }
            }
            // Unary operations
            if (auto *un = dynamic_cast<UnaryOpNode*>(node)) {
                VarType ot = typeOf(un->operand.get());
                switch (un->op) {
                case UnaryOp::INCREMENT: case UnaryOp::DECREMENT:
                    if (ot==VarType::INT||ot==VarType::FLOAT) return ot;
                    break;
                case UnaryOp::ABS:
                    if (ot==VarType::INT||ot==VarType::FLOAT) return ot;
                    break;
                case UnaryOp::LENGTH:
                    if (ot==VarType::ARRAY||ot==VarType::STRING) return VarType::INT;
                    break;
                case UnaryOp::MIN: case UnaryOp::MAX:
                    if (ot==VarType::ARRAY) return VarType::ARRAY;
                    break;
                default: break;
                }
                report(InvalidOperation, varTypeName(ot));
                return VarType::ERROR;
            }
            // Arrays
            if (auto *arr = dynamic_cast<ArrayNode*>(node)) {
                if (arr->elements.empty()) return VarType::ARRAY;
                VarType et = typeOf(arr->elements[0].get());
                for (auto &e : arr->elements) {
                    VarType t2 = typeOf(e.get());
                    if (t2 != et) report(TypeMismatch, "array elements");
                }
                return VarType::ARRAY;
            }
            // Array access
            if (auto *acc = dynamic_cast<ArrayAccessNode*>(node)) {
                VarType at = typeOf(acc->index.get());
                VarType arrt = VarTypes.lookup(acc->arrayName);
                if (arrt != VarType::ARRAY) report(TypeMismatch, acc->arrayName);
                if (at != VarType::INT) report(TypeMismatch, "index type");
                return VarType::ARRAY;
            }
            // Print
            if (auto *p = dynamic_cast<PrintNode*>(node)) {
                return typeOf(p->expr.get());
            }
            // Default: error
            return VarType::ERROR;
        }

    public:
        bool hasError() const { return HasError; }

        // Program and Block
        void visit(ProgramNode &node) override {
            for (auto &s : node.statements) s->accept(*this);
        }
        void visit(BlockNode &node) override {
            for (auto &s : node.statements) s->accept(*this);
        }

        // Variable declarations
        void visit(MultiVarDeclNode &node) override {
            for (auto &decl : node.declarations) {
                const std::string &name = decl->name;
                if (VarTypes.count(name)) report(AlreadyDefined, name);
                else {
                    VarTypes[name] = decl->type;
                    if (decl->value) decl->value->accept(*this);
                }
            }
        }

        // Assignments
        void visit(AssignNode &node) override {
            if (!VarTypes.count(node.target)) report(NotDefined, node.target);
            node.value->accept(*this);
            VarType vt = VarTypes.lookup(node.target);
            VarType rt = typeOf(node.value.get());
            // Allowed ops per type
            bool ok = false;
            switch (vt) {
            case VarType::INT: case VarType::FLOAT:
                ok = true; break;
            case VarType::CHAR: case VarType::STRING: case VarType::BOOL: case VarType::ARRAY:
                ok = (node.op == BinaryOp::EQUAL);
                break;
            default: break;
            }
            if (!ok) report(InvalidOperation, varTypeName(vt));
            if (rt != vt && !(vt==VarType::FLOAT && rt==VarType::INT))
                report(TypeMismatch, node.target);
        }

        // Variable reference
        void visit(VarRefNode &node) override {
            if (!VarTypes.count(node.name)) report(NotDefined, node.name);
        }

        // Binary ops
        void visit(BinaryOpNode &node) override {
            node.left->accept(*this);
            node.right->accept(*this);
            if (node.op == BinaryOp::DIVIDE) {
                if (auto *lit = dynamic_cast<LiteralNode<int>*>(node.right.get())) {
                    if (lit->value == 0) report(DivideByZero, "");
                }
            }
            typeOf(&node);
        }

        // Unary ops
        void visit(UnaryOpNode &node) override {
            node.operand->accept(*this);
            typeOf(&node);
        }

        // Control structures
        void visit(IfElseNode &node) override {
            node.condition->accept(*this);
            if (typeOf(node.condition.get()) != VarType::BOOL)
                report(TypeMismatch, "if condition");
            node.thenBlock->accept(*this);
            if (node.elseBlock) node.elseBlock->accept(*this);
        }
        void visit(ForLoopNode &node) override {
            if (node.init)  node.init->accept(*this);
            if (node.condition) {
                node.condition->accept(*this);
                if (typeOf(node.condition.get()) != VarType::BOOL)
                    report(TypeMismatch, "for condition");
            }
            if (node.update) node.update->accept(*this);
            node.body->accept(*this);
        }
        void visit(WhileLoopNode &node) override {
            node.condition->accept(*this);
            if (typeOf(node.condition.get()) != VarType::BOOL)
                report(TypeMismatch, "while condition");
            node.body->accept(*this);
        }

        // Print
        void visit(PrintNode &node) override {
            node.expr->accept(*this);
        }

        // Array
        void visit(ArrayNode &node) override {
            for (auto &e : node.elements) e->accept(*this);
            typeOf(&node);
        }
        void visit(ArrayAccessNode &node) override {
            node.index->accept(*this);
            typeOf(&node);
        }

        // Concat, Pow
        void visit(ConcatNode &node) override {
            node.left->accept(*this);
            node.right->accept(*this);
            typeOf(&node);
        }
        void visit(PowNode &node) override {
            node.base->accept(*this);
            node.exponent->accept(*this);
            typeOf(&node);
        }
    };
}

bool Semantic::semantic(ProgramNode *root) {
    if (!root) return false;
    DeclCheck checker;
    root->accept(checker);
    return checker.hasError();
}
