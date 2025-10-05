#include "ir_generator.h"
#include <cassert>

static std::string opToLlvm(const std::string& op) {
    if (op == "+") return "add";
    if (op == "-") return "sub";
    if (op == "*") return "mul";
    if (op == "/") return "sdiv";
    if (op == "%") return "srem";
    if (op == "<") return "icmp slt";
    if (op == ">") return "icmp sgt";
    if (op == "<=") return "icmp sle";
    if (op == ">=") return "icmp sge";
    if (op == "==") return "icmp eq";
    if (op == "!=") return "icmp ne";
    return "";
}

void IRGenerator::ensureBlock(FunctionContext& fn) {
    if (fn.currentLabel.empty()) {
        fn.currentLabel = "entry";
        fn.body << fn.currentLabel << ":\n";
    }
}

void IRGenerator::startBlock(FunctionContext& fn, const std::string& label) {
    fn.currentLabel = label;
    fn.currentTerminated = false;
    fn.body << label << ":\n";
}

void IRGenerator::branch(FunctionContext& fn, const std::string& target) {
    if (!fn.currentTerminated) {
        fn.body << "  br label %" << target << "\n";
        fn.currentTerminated = true;
    }
}

void IRGenerator::cbranch(FunctionContext& fn, const IRValue& cond, const std::string& tlabel, const std::string& flabel) {
    ensureBlock(fn);
    fn.body << "  br i1 " << cond.reg << ", label %" << tlabel << ", label %" << flabel << "\n";
    fn.currentTerminated = true;
}

IRValue IRGenerator::toBool(const IRValue& v, FunctionContext& fn) {
    if (v.type == "i1") return v;
    IRValue out; out.type = "i1"; out.reg = newTemp(fn);
    fn.body << "  " << out.reg << " = icmp ne i32 " << v.reg << ", 0\n";
    return out;
}

std::string IRGenerator::ensureAlloca(const std::string& name, FunctionContext& fn) {
    auto it = fn.locals.find(name);
    if (it != fn.locals.end()) return it->second;
    std::string a = newTemp(fn);
    fn.entryAllocas.push_back("  " + a + " = alloca i32\n");
    fn.locals[name] = a;
    return a;
}

IRValue IRGenerator::getStringPtr(const std::string& s, FunctionContext& fn) {
    auto it = strToGlobal.find(s);
    std::string g;
    if (it == strToGlobal.end()) {
        std::string name = "@.str" + std::to_string(strToGlobal.size());
        // escape string content
        std::string esc; esc.reserve(s.size()*2);
        for (unsigned char c : s) {
            if (c == '\n') esc += "\\0A"; else if (c == '\t') esc += "\\09"; else if (c == '\\') esc += "\\5C"; else if (c == '"') esc += "\\22"; else esc += c;
        }
        size_t n = s.size()+1;
        globalDefs.push_back(name + " = private unnamed_addr constant [" + std::to_string(n) + " x i8] c\"" + esc + "\\00\", align 1\n");
        strToGlobal.emplace(s, name);
        g = name;
    } else {
        g = it->second;
    }
    IRValue out; out.type = "i8*"; out.reg = newTemp(fn);
    size_t n = s.size()+1;
    fn.body << "  " << out.reg << " = getelementptr inbounds [" << n << " x i8], [" << n << " x i8]* " << g << ", i64 0, i64 0\n";
    return out;
}

IRValue IRGenerator::lvalueAddress(const Expr* e, FunctionContext& fn) {
    if (auto v = dynamic_cast<const VarExpr*>(e)) {
        std::string a = ensureAlloca(v->name, fn);
        return IRValue{a, "i32*"};
    }
    assert(false && "unsupported lvalue");
    return IRValue{"", ""};
}

IRValue IRGenerator::emitExpr(const Expr* e, FunctionContext& fn) {
    if (auto num = dynamic_cast<const NumberExpr*>(e)) {
        return IRValue{std::to_string(num->value), "i32"};
    }
    if (auto s = dynamic_cast<const StringLiteralExpr*>(e)) {
        return getStringPtr(s->value, fn);
    }
    if (auto bin = dynamic_cast<const BinaryExpr*>(e)) {
        if (bin->op == "&&" || bin->op == "||") {
            IRValue l = emitExpr(bin->lhs.get(), fn);
            l = toBool(l, fn);
            IRValue r = emitExpr(bin->rhs.get(), fn);
            r = toBool(r, fn);
            IRValue out; out.type = "i1"; out.reg = newTemp(fn);
            if (bin->op == "&&") {
                fn.body << "  " << out.reg << " = and i1 " << l.reg << ", " << r.reg << "\n";
            } else {
                fn.body << "  " << out.reg << " = or i1 " << l.reg << ", " << r.reg << "\n";
            }
            // normalize to i32 for now
            IRValue z; z.type = "i32"; z.reg = newTemp(fn);
            fn.body << "  " << z.reg << " = zext i1 " << out.reg << " to i32\n";
            return z;
        }
        IRValue l = emitExpr(bin->lhs.get(), fn);
        IRValue r = emitExpr(bin->rhs.get(), fn);
        std::string op = opToLlvm(bin->op);
        if (op.rfind("icmp", 0) == 0) {
            IRValue cmp; cmp.type = "i1"; cmp.reg = newTemp(fn);
            fn.body << "  " << cmp.reg << " = " << op << " i32 " << l.reg << ", " << r.reg << "\n";
            IRValue z; z.type = "i32"; z.reg = newTemp(fn);
            fn.body << "  " << z.reg << " = zext i1 " << cmp.reg << " to i32\n";
            return z;
        }
        IRValue out; out.type = "i32"; out.reg = newTemp(fn);
        fn.body << "  " << out.reg << " = " << op << " i32 " << l.reg << ", " << r.reg << "\n";
        return out;
    }
    if (auto un = dynamic_cast<const UnaryExpr*>(e)) {
        IRValue v = emitExpr(un->operand.get(), fn);
        if (un->op == "-") {
            IRValue out; out.type = "i32"; out.reg = newTemp(fn);
            fn.body << "  " << out.reg << " = sub i32 0, " << v.reg << "\n";
            return out;
        }
        if (un->op == "!") {
            IRValue cmp; cmp.type = "i1"; cmp.reg = newTemp(fn);
            fn.body << "  " << cmp.reg << " = icmp eq i32 " << v.reg << ", 0\n";
            IRValue z; z.type = "i32"; z.reg = newTemp(fn);
            fn.body << "  " << z.reg << " = zext i1 " << cmp.reg << " to i32\n";
            return z;
        }
    }
    if (auto asn = dynamic_cast<const AssignExpr*>(e)) {
        IRValue addr = lvalueAddress(asn->target.get(), fn);
        IRValue val = emitExpr(asn->value.get(), fn);
        fn.body << "  store i32 " << val.reg << ", i32* " << addr.reg << "\n";
        return val;
    }
    if (auto call = dynamic_cast<const CallExpr*>(e)) {
        if (auto calleeVar = dynamic_cast<VarExpr*>(call->callee.get())) {
            const std::string& name = calleeVar->name;
            if (name == "printf") {
                IRValue fmt = emitExpr(call->args[0].get(), fn);
                IRValue out; out.type = "i32"; out.reg = newTemp(fn);
                fn.body << "  " << out.reg << " = call i32 (i8*, ...) @printf(i8* " << fmt.reg;
                for (size_t i=1;i<call->args.size();++i) {
                    IRValue ai = emitExpr(call->args[i].get(), fn);
                    fn.body << ", i32 " << ai.reg;
                }
                fn.body << ")\n";
                return out;
            }
            if (name == "scanf") {
                IRValue fmt = emitExpr(call->args[0].get(), fn);
                IRValue out; out.type = "i32"; out.reg = newTemp(fn);
                fn.body << "  " << out.reg << " = call i32 (i8*, ...) @scanf(i8* " << fmt.reg << ")\n";
                return out;
            }
            if (name == "malloc") {
                usedMalloc = true;
                IRValue sz = emitExpr(call->args[0].get(), fn);
                IRValue out; out.type = "i8*"; out.reg = newTemp(fn);
                fn.body << "  " << out.reg << " = call i8* @malloc(i64 " << sz.reg << ")\n";
                return out;
            }
            if (name == "free") {
                usedFree = true;
                IRValue p = emitExpr(call->args[0].get(), fn);
                fn.body << "  call void @free(i8* " << p.reg << ")\n";
                return IRValue{"0","i32"};
            }
        }
        // Generic direct call with i32 args and i32 return
        IRValue out; out.type = "i32"; out.reg = newTemp(fn);
        fn.body << "  " << out.reg << " = call i32 @unknown()\n";
        return out;
    }
    assert(false && "unsupported expr");
    return IRValue{"0","i32"};
}

void IRGenerator::emitStmt(const Stmt* s, FunctionContext& fn) {
    if (auto r = dynamic_cast<const ReturnStmt*>(s)) {
        IRValue v = emitExpr(r->value.get(), fn);
        fn.body << "  ret i32 " << v.reg << "\n";
        fn.currentTerminated = true;
        return;
    }
    if (auto e = dynamic_cast<const ExprStmt*>(s)) {
        if (e->expr) (void)emitExpr(e->expr.get(), fn);
        return;
    }
    if (auto w = dynamic_cast<const WhileStmt*>(s)) {
        std::string condL = newLabel(fn, "while.cond");
        std::string bodyL = newLabel(fn, "while.body");
        std::string endL  = newLabel(fn, "while.end");
        branch(fn, condL);
        fn.currentLabel = condL; fn.currentTerminated = false; fn.body << condL << ":\n";
        IRValue c = toBool(emitExpr(w->condition.get(), fn), fn);
        cbranch(fn, c, bodyL, endL);
        fn.currentLabel = bodyL; fn.currentTerminated = false; fn.body << bodyL << ":\n";
        emitStmt(w->body.get(), fn);
        branch(fn, condL);
        fn.currentLabel = endL; fn.currentTerminated = false; fn.body << endL << ":\n";
        return;
    }
    if (auto i = dynamic_cast<const IfStmt*>(s)) {
        std::string thenL = newLabel(fn, "if.then");
        std::string elseL = newLabel(fn, "if.else");
        std::string endL  = newLabel(fn, "if.end");
        IRValue c = toBool(emitExpr(i->condition.get(), fn), fn);
        cbranch(fn, c, thenL, (i->elseBranch?elseL:endL));
        fn.currentLabel = thenL; fn.currentTerminated = false; fn.body << thenL << ":\n";
        emitStmt(i->thenBranch.get(), fn);
        branch(fn, endL);
        if (i->elseBranch) {
            fn.currentLabel = elseL; fn.currentTerminated = false; fn.body << elseL << ":\n";
            emitStmt(i->elseBranch.get(), fn);
            branch(fn, endL);
        }
        fn.currentLabel = endL; fn.currentTerminated = false; fn.body << endL << ":\n";
        return;
    }
    if (auto b = dynamic_cast<const BlockStmt*>(s)) {
        emitBlock(b, fn);
        return;
    }
    if (auto brk = dynamic_cast<const BreakStmt*>(s)) {
        if (!fn.loopStack.empty()) branch(fn, fn.loopStack.back().breakLabel);
        return;
    }
    if (auto cont = dynamic_cast<const ContinueStmt*>(s)) {
        if (!fn.loopStack.empty()) branch(fn, fn.loopStack.back().continueLabel);
        return;
    }
    if (auto g = dynamic_cast<const GotoStmt*>(s)) {
        branch(fn, getOrCreateLabel(fn, g->label));
        return;
    }
    if (auto lab = dynamic_cast<const LabelStmt*>(s)) {
        startBlock(fn, getOrCreateLabel(fn, lab->label));
        return;
    }
}

void IRGenerator::emitBlock(const BlockStmt* blk, FunctionContext& fn) {
    for (const auto& st : blk->statements) {
        if (fn.currentTerminated) break;
        emitStmt(st.get(), fn);
    }
}

std::string IRGenerator::getOrCreateLabel(FunctionContext& fn, const std::string& userLabel) {
    auto it = fn.labelMap.find(userLabel);
    if (it != fn.labelMap.end()) return it->second;
    std::string l = userLabel + ".L" + std::to_string(fn.blockCounter++);
    fn.labelMap.emplace(userLabel, l);
    return l;
}

std::string IRGenerator::generateModuleIR(const Function& fn) {
    FunctionContext ctx;
    // entry label will be auto-created
    ctx.currentLabel.clear();

    std::ostringstream out;
    // Emit header
    out << "; ModuleID = 'my_compiler'\n";
    out << "source_filename = \"my_compiler\"\n\n";

    // Function signature (assume i32 () for now)
    out << "define i32 @" << fn.name << "() {\n";

    // Prologue
    ensureBlock(ctx);

    // Emit entry allocas later
    // Body
    if (fn.bodyBlock) {
        emitBlock(fn.bodyBlock.get(), ctx);
    } else if (!fn.body.empty()) {
        // Legacy single return
        if (auto ret = dynamic_cast<ReturnStmt*>(fn.body.front().get())) {
            IRValue v = emitExpr(ret->value.get(), ctx);
            ctx.body << "  ret i32 " << v.reg << "\n";
        } else {
            ctx.body << "  ret i32 0\n";
        }
    } else {
        ctx.body << "  ret i32 0\n";
    }

    // If the block wasn't terminated, ensure a return
    if (!ctx.currentTerminated) {
        ctx.body << "  ret i32 0\n";
    }

    // Splice entry allocas at top
    std::ostringstream bodyFull;
    bodyFull << "entry:\n";
    for (auto& a : ctx.entryAllocas) bodyFull << a;
    bodyFull << ctx.body.str();
    out << bodyFull.str();
    out << "}\n\n";

    // Globals
    for (auto& g : globalDefs) out << g;

    // Declares
    out << "declare i32 @printf(i8*, ...)\n";
    out << "declare i32 @scanf(i8*, ...)\n";
    if (usedMalloc) out << "declare noalias i8* @malloc(i64)\n";
    if (usedFree) out << "declare void @free(i8*)\n";
    return out.str();
}
