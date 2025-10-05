#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct SymbolInfo {
    std::string typeName;
};

class SymbolTable {
public:
    void pushScope();
    void popScope();
    bool declare(const std::string& name, const SymbolInfo& info);
    const SymbolInfo* lookup(const std::string& name) const;
private:
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
};
