#pragma once
#include "ast.h"
#include "error_handler.h"

// Performs semantic validation on a single function (legacy helper)
void semanticCheck(const Function& fn, ErrorHandler& err);

// Performs full module-level semantic validation:
// - variable declarations and scopes
// - type inference/checking for expressions, implicit casts
// - array-to-pointer decay, pointer arithmetic scaling, member access
// - function signature checking (arity and types)
// - typedef-based declarations (ints, function pointers)
void semanticCheckModule(const std::vector<std::unique_ptr<Function>>& fns, ErrorHandler& err);
