#include "symbol_table.h"

void SymbolTable::pushScope() {
    scopes.emplace_back();
}

void SymbolTable::popScope() {
    if (!scopes.empty()) scopes.pop_back();
}

bool SymbolTable::declare(const std::string& name, const SymbolInfo& info) {
    if (scopes.empty()) scopes.emplace_back();
    auto& top = scopes.back();
    if (top.count(name)) return false;
    top[name] = info;
    return true;
}

const SymbolInfo* SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}
