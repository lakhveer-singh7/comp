#pragma once
#include <memory>
#include "ast.h"
#include <string>

class IRGenerator {
public:
    std::string generateModuleIR(const Function& fn);
private:
    int emitExpr(const Expr* e, std::ostringstream& ir, int& tempCounter);
};
