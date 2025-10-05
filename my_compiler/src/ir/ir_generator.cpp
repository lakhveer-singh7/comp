#include "type_system.h"
std::string IRGenerator::typeToIR(Type* t) {
    if (!t) return "i32";
    switch (t->kind) {
        case TypeKind::Int: return "i32";
        case TypeKind::Char: return "i8";
        case TypeKind::Void: return "void";
        case TypeKind::Pointer: {
            std::string e = typeToIR(t->element);
            if (e.rfind("%struct.", 0) == 0) return e + "*";
            return e + "*";
        }
        case TypeKind::Array: {
            return "[" + std::to_string(t->arrayLength) + " x " + typeToIR(t->element) + "]";
        }
        case TypeKind::Struct: {
            ensureStructType(t->structName);
            return "%struct." + t->structName;
        }
        case TypeKind::Function: {
            // return type for signature contexts
            return typeToIR(t->element);
        }
    }
    return "i32";
}
#include "ir_generator.h"
#include <cassert>

static std::string opToLlvm(const std::string& op) {
    if (op == "+") return "add";
    if (op == "-") return "sub";
    if (op == "*") return "mul";
    if (op == "/") return "sdiv";
    if (op == "%") return "srem";
    if (op == "&") return "and";
    if (op == "|") return "or";
    if (op == "^") return "xor";
    if (op == "<<") return "shl";
    if (op == ">>") return "ashr";
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
    std::string ty = fn.localTypes.count(name) ? fn.localTypes[name] : std::string("i32");
    fn.entryAllocas.push_back("  " + a + " = alloca " + ty + "\n");
    fn.locals[name] = a;
    if (!fn.localTypes.count(name)) fn.localTypes[name] = ty;
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
        // Prefer global if present
        auto git = globalVars.find(v->name);
        if (git != globalVars.end()) {
            return IRValue{git->second, "i32*"};
        }
        std::string a = ensureAlloca(v->name, fn);
        return IRValue{a, "i32*"};
    }
    if (auto m = dynamic_cast<const MemberExpr*>(e)) {
        // base.field where base is a local/global struct variable
        std::string sname;
        std::string basePtr;
        bool isGlobal = false;
        if (auto bv = dynamic_cast<VarExpr*>(m->base.get())) {
            auto itL = fn.localStructName.find(bv->name);
            if (itL != fn.localStructName.end()) { sname = itL->second; basePtr = fn.locals[bv->name]; }
            auto itG = globalStructName.find(bv->name);
            if (sname.empty() && itG != globalStructName.end()) { sname = itG->second; basePtr = globalVars[bv->name]; isGlobal = true; }
        }
        if (!sname.empty()) {
            ensureStructType(sname);
            // lookup field index and type
            extern std::unordered_map<std::string, std::vector<std::pair<std::string,std::string>>> g_struct_field_types;
            size_t idx = 0; std::string fty = "i32";
            auto &vec = g_struct_field_types[sname];
            for (size_t i=0;i<vec.size();++i) { if (vec[i].first == m->field) { idx = i; fty = vec[i].second; break; } }
            IRValue gep; gep.type = fty + "*"; gep.reg = newTemp(fn);
            if (isGlobal) {
                fn.body << "  " << gep.reg << " = getelementptr inbounds %struct." << sname << ", %struct." << sname << "* " << basePtr << ", i32 0, i32 " << idx << "\n";
            } else {
                fn.body << "  " << gep.reg << " = getelementptr inbounds %struct." << sname << ", %struct." << sname << "* " << basePtr << ", i32 0, i32 " << idx << "\n";
            }
            return gep;
        }
    }
    if (auto pm = dynamic_cast<const PtrMemberExpr*>(e)) {
        // base->field where base is a pointer to struct
        IRValue base = emitExpr(pm->base.get(), fn);
        // Heuristic: if base.type is %struct.X*, extract X
        std::string sname;
        if (base.type.rfind("%struct.", 0) == 0) {
            auto pos = base.type.find('*');
            sname = base.type.substr(std::string("%struct.").size(), pos - std::string("%struct.").size());
        }
        ensureStructType(sname);
        extern std::unordered_map<std::string, std::vector<std::pair<std::string,std::string>>> g_struct_field_types;
        size_t idx = 0; std::string fty = "i32";
        auto &vec = g_struct_field_types[sname];
        for (size_t i=0;i<vec.size();++i) { if (vec[i].first == pm->field) { idx = i; fty = vec[i].second; break; } }
        IRValue gep; gep.type = fty + "*"; gep.reg = newTemp(fn);
        fn.body << "  " << gep.reg << " = getelementptr inbounds %struct." << (sname.empty()?std::string("S"):sname) << ", %struct." << (sname.empty()?std::string("S"):sname) << "* " << base.reg << ", i32 0, i32 " << idx << "\n";
        return gep;
    }
    if (auto idx = dynamic_cast<const ArrayIndexExpr*>(e)) {
        // Base can be local array name or a pointer
        if (auto v = dynamic_cast<VarExpr*>(idx->base.get())) {
            auto it = fn.localArrayLen.find(v->name);
            IRValue iv = emitExpr(idx->index.get(), fn);
            // Cast index to i64
            IRValue idx64; idx64.type = "i64"; idx64.reg = newTemp(fn);
            fn.body << "  " << idx64.reg << " = zext i32 " << iv.reg << " to i64\n";
            if (it != fn.localArrayLen.end()) {
                // GEP into [N x i32]
                std::string arr = fn.locals[v->name];
                std::string elemTy = fn.localArrayElem.count(v->name) ? fn.localArrayElem[v->name] : std::string("i32");
                IRValue eltPtr; eltPtr.type = elemTy + "*"; eltPtr.reg = newTemp(fn);
                fn.body << "  " << eltPtr.reg << " = getelementptr inbounds [" << it->second << " x " << elemTy << "], [" << it->second << " x " << elemTy << "]* " << arr << ", i64 0, i64 " << idx64.reg << "\n";
                return eltPtr;
            } else {
                // Treat as pointer i32*
                IRValue basePtr = emitExpr(idx->base.get(), fn);
                std::string elemTy = (basePtr.type == "i8*") ? std::string("i8") : std::string("i32");
                IRValue eltPtr; eltPtr.type = elemTy + "*"; eltPtr.reg = newTemp(fn);
                fn.body << "  " << eltPtr.reg << " = getelementptr inbounds " << elemTy << ", " << elemTy << "* " << basePtr.reg << ", i64 " << idx64.reg << "\n";
                return eltPtr;
            }
        } else {
            // Pointer base
            IRValue basePtr = emitExpr(idx->base.get(), fn);
            IRValue iv = emitExpr(idx->index.get(), fn);
            IRValue idx64; idx64.type = "i64"; idx64.reg = newTemp(fn);
            fn.body << "  " << idx64.reg << " = zext i32 " << iv.reg << " to i64\n";
            std::string elemTy = (basePtr.type == "i8*") ? std::string("i8") : std::string("i32");
            IRValue eltPtr; eltPtr.type = elemTy + "*"; eltPtr.reg = newTemp(fn);
            fn.body << "  " << eltPtr.reg << " = getelementptr inbounds " << elemTy << ", " << elemTy << "* " << basePtr.reg << ", i64 " << idx64.reg << "\n";
            return eltPtr;
        }
    }
    assert(false && "unsupported lvalue");
    return IRValue{"", ""};
}
void IRGenerator::ensureStructType(const std::string& name) {
    if (name.empty()) return;
    if (usedStructs.count(name)) return;
    usedStructs.insert(name);
    // Query parser-registered struct field types
    extern std::unordered_map<std::string, std::vector<std::pair<std::string,std::string>>> g_struct_field_types;
    auto it = g_struct_field_types.find(name);
    if (it == g_struct_field_types.end() || it->second.empty()) {
        structTypeDefs.push_back("%struct." + name + " = type { i32 }\n");
        return;
    }
    std::ostringstream ty;
    ty << "%struct." << name << " = type { ";
    for (size_t i=0;i<it->second.size();++i) {
        if (i) ty << ", ";
        ty << it->second[i].second; // IR type like i32/i8
    }
    ty << " }\n";
    structTypeDefs.push_back(ty.str());
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
    if (auto v = dynamic_cast<const VarExpr*>(e)) {
        // Load from local or global
        auto git = globalVars.find(v->name);
        IRValue out; out.type = "i32"; out.reg = newTemp(fn);
        if (git != globalVars.end()) {
            std::string ty = globalVarTypes[v->name].empty() ? std::string("i32") : globalVarTypes[v->name];
            fn.body << "  " << out.reg << " = load " << ty << ", " << ty << "* " << git->second << "\n";
            out.type = ty;
        } else {
            std::string a = ensureAlloca(v->name, fn);
            std::string ty = fn.localTypes[v->name].empty() ? std::string("i32") : fn.localTypes[v->name];
            fn.body << "  " << out.reg << " = load " << ty << ", " << ty << "* " << a << "\n";
            out.type = ty;
        }
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
        if (un->op == "&") {
            IRValue addr = lvalueAddress(un->operand.get(), fn);
            return addr;
        }
        if (un->op == "*") {
            // load i32 from pointer
            IRValue out; out.type = "i32"; out.reg = newTemp(fn);
            fn.body << "  " << out.reg << " = load i32, i32* " << v.reg << "\n";
            return out;
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
                    fn.body << ", " << ai.type << " " << ai.reg;
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
            // Known function definitions or externs
            auto sigIt = funcDecls.find(name);
            if (sigIt != funcDecls.end()) {
                IRValue out; out.type = "i32"; out.reg = newTemp(fn);
                fn.body << "  " << out.reg << " = call i32 @" << name << "(";
                for (size_t i=0;i<call->args.size();++i) {
                    IRValue ai = emitExpr(call->args[i].get(), fn);
                    if (i) fn.body << ", ";
                    fn.body << ai.type << " " << ai.reg;
                }
                fn.body << ")\n";
                usedFunctions.insert(name);
                return out;
            }
        }
        // Generic direct call with i32 args and i32 return
        IRValue out; out.type = "i32"; out.reg = newTemp(fn);
        // Try function pointer call
        IRValue cal = emitExpr(call->callee.get(), fn);
        fn.body << "  " << out.reg << " = call i32 " << cal.reg << "(";
        for (size_t i=0;i<call->args.size();++i) {
            IRValue ai = emitExpr(call->args[i].get(), fn);
            if (i) fn.body << ", ";
            fn.body << ai.type << " " << ai.reg;
        }
        fn.body << ")\n";
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
    if (auto dw = dynamic_cast<const DoWhileStmt*>(s)) {
        std::string bodyL = newLabel(fn, "do.body");
        std::string condL = newLabel(fn, "do.cond");
        std::string endL  = newLabel(fn, "do.end");
        branch(fn, bodyL);
        startBlock(fn, bodyL);
        fn.loopStack.push_back(LoopTargets{condL, endL});
        emitStmt(dw->body.get(), fn);
        fn.loopStack.pop_back();
        branch(fn, condL);
        startBlock(fn, condL);
        IRValue c = toBool(emitExpr(dw->condition.get(), fn), fn);
        cbranch(fn, c, bodyL, endL);
        startBlock(fn, endL);
        return;
    }
    if (auto f = dynamic_cast<const ForStmt*>(s)) {
        std::string condL = newLabel(fn, "for.cond");
        std::string bodyL = newLabel(fn, "for.body");
        std::string iterL = newLabel(fn, "for.iter");
        std::string endL  = newLabel(fn, "for.end");
        if (f->init) emitStmt(f->init.get(), fn);
        branch(fn, condL);
        startBlock(fn, condL);
        if (f->condition) {
            IRValue c = toBool(emitExpr(f->condition.get(), fn), fn);
            cbranch(fn, c, bodyL, endL);
        } else {
            branch(fn, bodyL);
        }
        startBlock(fn, bodyL);
        fn.loopStack.push_back(LoopTargets{iterL, endL});
        emitStmt(f->body.get(), fn);
        fn.loopStack.pop_back();
        branch(fn, iterL);
        startBlock(fn, iterL);
        if (f->iter) emitStmt(f->iter.get(), fn);
        branch(fn, condL);
        startBlock(fn, endL);
        return;
    }
    if (auto sw = dynamic_cast<const SwitchStmt*>(s)) {
        IRValue v = emitExpr(sw->value.get(), fn);
        std::string endL = newLabel(fn, "switch.end");
        std::string defaultL = sw->defaultBody.empty() ? endL : newLabel(fn, "switch.default");
        // Prepare labels for cases
        std::vector<std::pair<long,std::string>> caseLabels;
        for (size_t i=0;i<sw->cases.size();++i) {
            caseLabels.emplace_back(sw->cases[i].value, newLabel(fn, "switch.case"));
        }
        // Emit switch header
        fn.body << "  switch i32 " << v.reg << ", label %" << defaultL << " [\n";
        for (auto& kv : caseLabels) {
            fn.body << "    i32 " << kv.first << ", label %" << kv.second << "\n";
        }
        fn.body << "  ]\n";
        // Push break target
        fn.loopStack.push_back(LoopTargets{"", endL});
        // Emit cases
        for (size_t i=0;i<sw->cases.size();++i) {
            startBlock(fn, caseLabels[i].second);
            for (const auto& st : sw->cases[i].statements) {
                if (fn.currentTerminated) break;
                emitStmt(st.get(), fn);
            }
            // fallthrough allowed: no forced branch
        }
        // Default
        if (defaultL != endL) {
            startBlock(fn, defaultL);
            for (const auto& st : sw->defaultBody) {
                if (fn.currentTerminated) break;
                emitStmt(st.get(), fn);
            }
        }
        // End
        startBlock(fn, endL);
        fn.loopStack.pop_back();
        return;
    }
    if (auto vd = dynamic_cast<const VarDeclStmt*>(s)) {
        if (vd->isStatic) {
            // global variable
            std::string g = sanitizeGlobal(vd->name);
            if (!globalVars.count(vd->name)) {
                long init = 0;
                if (vd->init) {
                    if (auto num = dynamic_cast<NumberExpr*>(vd->init.get())) init = num->value;
                }
                std::string gty = "i32";
                if (vd->type && vd->type->kind == TypeKind::Char) gty = "i8";
                globalDefs.push_back(g + " = internal global " + gty + " " + std::to_string(init) + ", align 4\n");
                globalVars[vd->name] = g;
                globalVarTypes[vd->name] = gty;
            }
            if (vd->init && !dynamic_cast<NumberExpr*>(vd->init.get())) {
                // runtime init: store at entry
                IRValue a; a.type = globalVarTypes[vd->name] + "*"; a.reg = g;
                IRValue val = emitExpr(vd->init.get(), fn);
                fn.body << "  store " << globalVarTypes[vd->name] << " " << val.reg << ", " << globalVarTypes[vd->name] << "* " << a.reg << "\n";
            }
        } else {
            if (vd->type && vd->type->kind == TypeKind::Array) {
                // allocate array
                size_t n = vd->type->arrayLength;
                std::string a = newTemp(fn);
                std::string elem = (vd->type->element && vd->type->element->kind == TypeKind::Char) ? std::string("i8") : std::string("i32");
                fn.entryAllocas.push_back("  " + a + " = alloca [" + std::to_string(n) + " x " + elem + "]\n");
                fn.locals[vd->name] = a;
                fn.localArrayLen[vd->name] = n;
                fn.localTypes[vd->name] = "[" + std::to_string(n) + " x " + elem + "]";
                fn.localArrayElem[vd->name] = elem;
            } else {
                std::string a = ensureAlloca(vd->name, fn);
                if (vd->init) {
                    IRValue val = emitExpr(vd->init.get(), fn);
                    std::string ty = fn.localTypes[vd->name].empty() ? std::string("i32") : fn.localTypes[vd->name];
                    fn.body << "  store " << ty << " " << val.reg << ", " << ty << "* " << a << "\n";
                }
            }
        }
        return;
    }
}

void IRGenerator::emitBlock(const BlockStmt* blk, FunctionContext& fn) {
    for (const auto& st : blk->statements) {
        if (fn.currentTerminated) break;
        emitStmt(st.get(), fn);
    }
}

void IRGenerator::emitFunctionPrologue(const Function& fnNode, FunctionContext& fn) {
    // Map parameters to allocas and store incoming values
    for (size_t i = 0; i < fnNode.detailedParams.size(); ++i) {
        const auto& p = fnNode.detailedParams[i];
        std::string ty = typeToIR(p.type);
        std::string a = newTemp(fn);
        fn.entryAllocas.push_back("  " + a + " = alloca " + ty + "\n");
        fn.locals[p.name] = a;
        fn.localTypes[p.name] = ty;
        // Get LLVM argument name: %0, %1, ...
        std::string arg = "%" + std::to_string(i);
        fn.body << "  store " << ty << " " << arg << ", " << ty << "* " << a << "\n";
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

    // Function signature: i32 with i32 params
    out << "define i32 @" << fn.name << "(";
    for (size_t i=0;i<fn.detailedParams.size();++i) {
        if (i) out << ", ";
        out << "i32 %" << i;
    }
    out << ") {\n";

    // Prologue
    ensureBlock(ctx);

    // Emit entry allocas later
    emitFunctionPrologue(fn, ctx);
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

    // Struct type defs
    for (auto& td : structTypeDefs) out << td;
    // Globals
    for (auto& g : globalDefs) out << g;

    // Declares
    out << "declare i32 @printf(i8*, ...)\n";
    out << "declare i32 @scanf(i8*, ...)\n";
    if (usedMalloc) out << "declare noalias i8* @malloc(i64)\n";
    if (usedFree) out << "declare void @free(i8*)\n";
    return out.str();
}

std::string IRGenerator::generateModuleIR(const std::vector<std::unique_ptr<Function>>& fns) {
    // Reset module state
    strToGlobal.clear();
    globalDefs.clear();
    globalVars.clear();
    globalVarTypes.clear();
    usedMalloc = usedFree = false;
    usedFunctions.clear();

    std::ostringstream mod;
    mod << "; ModuleID = 'my_compiler'\n";
    mod << "source_filename = \"my_compiler\"\n\n";

    // Record function declarations for calls between functions
    for (const auto& fn : fns) {
        funcDecls[fn->name] = "i32"; // i32(...)
    }

    // Emit all functions
    for (const auto& fn : fns) {
        FunctionContext ctx;
        ctx.currentLabel.clear();

        std::string retIR = typeToIR(fn->returnType);
        mod << "define " << retIR << " @" << fn->name << "(";
        for (size_t i=0;i<fn->detailedParams.size();++i) {
            if (i) mod << ", ";
            mod << typeToIR(fn->detailedParams[i].type) << " %" << i;
        }
        mod << ") {\n";
        ensureBlock(ctx);
        emitFunctionPrologue(*fn, ctx);
        if (fn->bodyBlock) {
            emitBlock(fn->bodyBlock.get(), ctx);
        } else if (!fn->body.empty()) {
            if (auto ret = dynamic_cast<ReturnStmt*>(fn->body.front().get())) {
                IRValue v = emitExpr(ret->value.get(), ctx);
                ctx.body << "  ret " << retIR << " " << v.reg << "\n";
            } else {
                ctx.body << "  ret " << retIR << " 0\n";
            }
        } else {
            ctx.body << "  ret " << retIR << " 0\n";
        }
        if (!ctx.currentTerminated) {
            ctx.body << "  ret " << retIR << " 0\n";
        }
        std::ostringstream bodyFull;
        bodyFull << "entry:\n";
        for (auto& a : ctx.entryAllocas) bodyFull << a;
        bodyFull << ctx.body.str();
        mod << bodyFull.str();
        mod << "}\n\n";
    }

    for (auto& td : structTypeDefs) mod << td;
    for (auto& g : globalDefs) mod << g;
    mod << "declare i32 @printf(i8*, ...)\n";
    mod << "declare i32 @scanf(i8*, ...)\n";
    if (usedMalloc) mod << "declare noalias i8* @malloc(i64)\n";
    if (usedFree) mod << "declare void @free(i8*)\n";
    return mod.str();
}
