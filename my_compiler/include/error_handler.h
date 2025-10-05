#pragma once
#include <string>
#include <vector>
#include <iostream>

struct SourceLocation {
    int line = 1;
    int column = 1;
};

struct CompileError {
    SourceLocation loc;
    std::string message;
};

class ErrorHandler {
public:
    void report(const SourceLocation& loc, const std::string& message) {
        errors.push_back({loc, message});
    }

    bool hasErrors() const { return !errors.empty(); }

    void printAll(std::ostream& os = std::cerr) const {
        for (const auto& e : errors) {
            os << "error(" << e.loc.line << ":" << e.loc.column << "): " << e.message << "\n";
        }
    }

    size_t count() const { return errors.size(); }

private:
    std::vector<CompileError> errors;
};
