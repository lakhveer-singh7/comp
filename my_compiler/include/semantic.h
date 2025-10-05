#pragma once
#include "ast.h"
#include "error_handler.h"

// Performs basic semantic validation on a single function:
// - variable declarations before use
// - simple lvalue checks for assignment
// - control-flow: break/continue inside loops only
// - basic call arity check for printf/scanf (best-effort)
void semanticCheck(const Function& fn, ErrorHandler& err);
