#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "error_handler.h"
#include "lexer.h"
#include "parser.h"
#include "ir_generator.h"

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
    Lexer lex(source);
    Parser parser(lex, err);
    auto fn = parser.parseFunction();
    if (err.hasErrors() || !fn) {
        err.printAll();
        return 1;
    }

    IRGenerator irgen;
    std::string ir = irgen.generateModuleIR(*fn);

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
