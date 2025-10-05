#include "ir_generator.h"
#include <sstream>
#include <cassert>

int IRGenerator::emitExpr(const Expr* e, std::ostringstream& ir, int& t) {
    if (auto num = dynamic_cast<const NumberExpr*>(e)) {
        int id = t++;
        ir << "  %t" << id << " = add i32 0, " << num->value << "\n";
        return id;
    }
    if (auto bin = dynamic_cast<const BinaryExpr*>(e)) {
        int l = emitExpr(bin->lhs.get(), ir, t);
        int r = emitExpr(bin->rhs.get(), ir, t);
        int id = t++;
        switch (bin->op) {
            case '+': ir << "  %t" << id << " = add i32 %t" << l << ", %t" << r << "\n"; break;
            case '-': ir << "  %t" << id << " = sub i32 %t" << l << ", %t" << r << "\n"; break;
            case '*': ir << "  %t" << id << " = mul i32 %t" << l << ", %t" << r << "\n"; break;
            case '/': ir << "  %t" << id << " = sdiv i32 %t" << l << ", %t" << r << "\n"; break;
            default: assert(false && "unsupported op");
        }
        return id;
    }
    assert(false && "unsupported expr");
    return -1;
}

std::string IRGenerator::generateModuleIR(const Function& fn) {
    std::ostringstream ir;
    ir << "; ModuleID = 'my_compiler'\n";
    ir << "source_filename = \"my_compiler\"\n\n";
    ir << "define i32 @" << fn.name << "() {\n";
    ir << "entry:\n";
    int t = 0;
    if (!fn.body.empty()) {
        if (auto ret = dynamic_cast<ReturnStmt*>(fn.body.front().get())) {
            int v = emitExpr(ret->value.get(), ir, t);
            ir << "  ret i32 %t" << v << "\n";
        } else {
            ir << "  ret i32 0\n";
        }
    } else {
        ir << "  ret i32 0\n";
    }
    ir << "}\n";
    return ir.str();
}
