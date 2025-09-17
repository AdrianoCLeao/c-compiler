#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/parser.h"

// Exits with a non-zero status if semantic errors are encountered.
void resolve_variables(ASTNode *program);

#endif
