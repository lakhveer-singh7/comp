#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "error_handler.h"
#include "lexer.h"
#include "parser.tab.hh"
#include "ir_generator.h"
#include "semantic.h"

int main(int argc, char** argv) {
    std::string outputPath = "outputs/output.ll";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        }
    }

    // Read full input source
    std::ostringstream buf;
    if (argc > 1 && std::string(argv[argc - 2]) != "-o") {
        std::ifstream in(argv[argc - 1]);
        if (!in) {
            std::cerr << "error: cannot open input file: " << argv[argc - 1] << "\n";
            return 1;
        }
        buf << in.rdbuf();
    } else {
        // default demo program
        buf << "int main() { return 1+2*3; }\n";
    }
    std::string source = buf.str();

    ErrorHandler err;
#if USE_FLEX_BISON
    // Feed buffer into a temporary file stream for Flex/Bison
    std::string tmpPath = "build/tmp_input.mc";
    std::ofstream tmp(tmpPath);
    tmp << source;
    tmp.close();
    FILE* f = std::fopen(tmpPath.c_str(), "r");
    if (!f) {
        std::cerr << "error: cannot reopen temp input\n";
        return 1;
    }
    extern FILE* yyin;
    yyin = f;
    extern std::vector<std::unique_ptr<Function>> g_functions;
    if (yyparse() != 0) {
        std::cerr << "parse failed\n";
        std::fclose(f);
        return 1;
    }
    std::fclose(f);
    if (g_functions.empty()) {
        std::cerr << "no functions parsed\n";
        return 1;
    }
#else
    Lexer lex(source);
    Parser parser(lex, err);
    auto fn = parser.parseFunction();
    if (err.hasErrors() || !fn) {
        err.printAll();
        return 1;
    }
#endif

    ErrorHandler semErr;
    for (auto& fndef : g_functions) {
        semanticCheck(*fndef, semErr);
    }
    if (semErr.hasErrors()) { semErr.printAll(); return 1; }

    IRGenerator irgen;
    std::string ir = irgen.generateModuleIR(g_functions);

    std::ofstream out(outputPath);
    if (!out) {
        std::cerr << "error: cannot open output file: " << outputPath << "\n";
        return 1;
    }
    out << ir;
    out.close();
    std::cout << "Wrote IR to " << outputPath << "\n";
    return 0;
}
