#pragma once
#include <memory>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ast.h"

struct IRValue {
    std::string reg;   // e.g. %t3 or i32 literal folded
    std::string type;  // e.g. i32, i1, i8*, i32*
};

class IRGenerator {
public:
    std::string generateModuleIR(const Function& fn);
    std::string generateModuleIR(const std::vector<std::unique_ptr<Function>>& fns);

private:
    struct LoopTargets { std::string continueLabel; std::string breakLabel; };
    struct FunctionContext {
        std::ostringstream body;
        std::vector<std::string> entryAllocas; // emitted at function entry
        std::unordered_map<std::string, std::string> locals; // var -> %alloca
        std::unordered_map<std::string, std::string> localTypes; // var -> IR type (i32/i8/[N x i32])
        int tempCounter = 0;
        int blockCounter = 0;
        std::string currentLabel;
        bool currentTerminated = false;
        std::vector<LoopTargets> loopStack;
        std::unordered_map<std::string, std::string> labelMap; // user label -> llvm label
        std::unordered_map<std::string, size_t> localArrayLen; // local arrays length by name
        std::unordered_map<std::string, std::string> localArrayElem; // name -> element IR type (i32/i8)
        std::unordered_map<std::string, std::string> localStructName; // name -> struct tag
    };

    // Module-level globals for string literals
    std::unordered_map<std::string, std::string> strToGlobal;
    std::vector<std::string> globalDefs;
    std::unordered_map<std::string, std::string> globalVars; // name -> @g
    std::unordered_map<std::string, std::string> globalVarTypes; // name -> IR type (i32/i8)
    std::unordered_map<std::string, std::string> globalStructName; // name -> struct tag
    // Function signatures for inter-proc calls
    std::unordered_map<std::string, std::vector<std::string>> funcParamIR; // name -> param IR types
    std::unordered_map<std::string, std::string> funcRetIR;               // name -> return IR type
    bool usedMalloc = false;
    bool usedFree = false;
    std::unordered_set<std::string> usedFunctions;
    std::unordered_set<std::string> usedStructs;
    std::vector<std::string> structTypeDefs;

    // Expressions/statements
    IRValue emitExpr(const Expr* e, FunctionContext& fn);
    void emitStmt(const Stmt* s, FunctionContext& fn);
    void emitBlock(const BlockStmt* blk, FunctionContext& fn);
    void emitFunctionPrologue(const Function& fnNode, FunctionContext& fn);

    // Helpers
    std::string newTemp(FunctionContext& fn) { return "%t" + std::to_string(fn.tempCounter++); }
    std::string newLabel(FunctionContext& fn, const std::string& base) { return base + std::to_string(fn.blockCounter++); }
    void ensureBlock(FunctionContext& fn);
    void startBlock(FunctionContext& fn, const std::string& label);
    void branch(FunctionContext& fn, const std::string& target);
    void cbranch(FunctionContext& fn, const IRValue& cond, const std::string& tlabel, const std::string& flabel);
    IRValue toBool(const IRValue& v, FunctionContext& fn);
    std::string ensureAlloca(const std::string& name, FunctionContext& fn);
    IRValue lvalueAddress(const Expr* e, FunctionContext& fn);
    IRValue getStringPtr(const std::string& s, FunctionContext& fn);
    std::string getOrCreateLabel(FunctionContext& fn, const std::string& userLabel);
    std::string sanitizeGlobal(const std::string& name) { return "@" + name; }
    void ensureStructType(const std::string& name);
    std::string typeToIR(Type* t);
    IRValue ensureCast(const IRValue& v, const std::string& toType, FunctionContext& fn);
};
