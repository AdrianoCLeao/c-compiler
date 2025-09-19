#include "../../include/semantic/semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEMANTIC_ERROR(...)            \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
        exit(1);                        \
    } while (0)

typedef struct VarScope {
    char **names;
    char **resolved;
    size_t count;
    size_t capacity;
    struct VarScope *parent;
} VarScope;

typedef struct {
    VarScope *current;
    int next_unique;
    int loop_depth;
} ResolveContext;

static VarScope *scope_create(VarScope *parent) {
    VarScope *scope = (VarScope *)calloc(1, sizeof(VarScope));
    if (!scope) {
        SEMANTIC_ERROR("Out of memory while creating scope");
    }
    scope->parent = parent;
    return scope;
}

static void scope_destroy(VarScope *scope) {
    if (!scope) return;
    for (size_t i = 0; i < scope->count; i++) {
        free(scope->names[i]);
        // resolved pointers are owned by the AST
    }
    free(scope->names);
    free(scope->resolved);
    free(scope);
}

static void scope_push(ResolveContext *ctx) {
    ctx->current = scope_create(ctx->current);
}

static void scope_pop(ResolveContext *ctx) {
    VarScope *scope = ctx->current;
    if (!scope) return;
    ctx->current = scope->parent;
    scope_destroy(scope);
}

static void scope_ensure_capacity(VarScope *scope) {
    if (scope->count < scope->capacity) return;
    size_t new_cap = scope->capacity ? scope->capacity * 2 : 8;
    char **new_names = (char **)realloc(scope->names, new_cap * sizeof(char *));
    char **new_resolved = (char **)realloc(scope->resolved, new_cap * sizeof(char *));
    if (!new_names || !new_resolved) {
        free(new_names);
        free(new_resolved);
        SEMANTIC_ERROR("Out of memory while resolving variables");
    }
    scope->names = new_names;
    scope->resolved = new_resolved;
    scope->capacity = new_cap;
}

static int scope_contains(VarScope *scope, const char *name) {
    if (!scope) return 0;
    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->names[i], name) == 0) return 1;
    }
    return 0;
}

static void scope_add(ResolveContext *ctx, const char *name, char *resolved) {
    VarScope *scope = ctx->current;
    if (!scope) {
        SEMANTIC_ERROR("Semantic Error: declaration outside of any scope");
    }
    if (scope_contains(scope, name)) {
        SEMANTIC_ERROR("Semantic Error: redeclaration of '%s'", name);
    }
    scope_ensure_capacity(scope);
    scope->names[scope->count] = strdup(name);
    if (!scope->names[scope->count]) {
        SEMANTIC_ERROR("Out of memory while storing variable name");
    }
    scope->resolved[scope->count] = resolved;
    scope->count++;
}

static char *scope_lookup(ResolveContext *ctx, const char *name) {
    for (VarScope *scope = ctx->current; scope; scope = scope->parent) {
        for (size_t i = 0; i < scope->count; i++) {
            if (strcmp(scope->names[i], name) == 0) {
                return scope->resolved[i];
            }
        }
    }
    return NULL;
}

static char *make_unique_name(const char *original, int index) {
    size_t len = strlen(original);
    size_t buf_sz = len + 32;
    char *buffer = (char *)malloc(buf_sz);
    if (!buffer) {
        SEMANTIC_ERROR("Out of memory while generating variable name");
    }
    snprintf(buffer, buf_sz, "%s_%d", original, index);
    return buffer;
}

static char *duplicate_owned(const char *value) {
    char *copy = strdup(value);
    if (!copy) {
        SEMANTIC_ERROR("Out of memory while duplicating string");
    }
    return copy;
}

static void set_node_value(ASTNode *node, const char *value) {
    if (!node) return;
    if (node->value && node->owns_value) {
        free(node->value);
    }
    if (value) {
        node->value = duplicate_owned(value);
        node->owns_value = true;
    } else {
        node->value = NULL;
        node->owns_value = false;
    }
}

static void set_node_value_transfer(ASTNode *node, char *value) {
    if (!node) return;
    if (node->value && node->owns_value) {
        free(node->value);
    }
    node->value = value;
    node->owns_value = true;
}

static void resolve_block_items(ASTNode *item, ResolveContext *ctx);
static void resolve_statement(ASTNode *stmt, ResolveContext *ctx);
static void resolve_expression(ASTNode *expr, ResolveContext *ctx);

static void resolve_declaration(ASTNode *decl, ResolveContext *ctx) {
    if (!decl || decl->type != AST_DECLARATION) return;
    if (!decl->value) {
        SEMANTIC_ERROR("Semantic Error: declaration missing identifier");
    }

    char *resolved = make_unique_name(decl->value, ctx->next_unique++);
    scope_add(ctx, decl->value, resolved);
    set_node_value_transfer(decl, resolved);

    if (decl->left) {
        resolve_expression(decl->left, ctx);
    }
}

static void resolve_statement(ASTNode *stmt, ResolveContext *ctx) {
    if (!stmt) return;
    switch (stmt->type) {
        case AST_STATEMENT_RETURN:
        case AST_STATEMENT_EXPRESSION:
            resolve_expression(stmt->left, ctx);
            break;
        case AST_STATEMENT_NULL:
            break;
        case AST_STATEMENT_IF:
            resolve_expression(stmt->left, ctx);
            resolve_statement(stmt->right, ctx);
            resolve_statement(stmt->third, ctx);
            break;
        case AST_STATEMENT_COMPOUND:
            scope_push(ctx);
            resolve_block_items(stmt->left, ctx);
            scope_pop(ctx);
            break;
        case AST_STATEMENT_WHILE:
            resolve_expression(stmt->left, ctx);
            ctx->loop_depth++;
            resolve_statement(stmt->right, ctx);
            ctx->loop_depth--;
            break;
        case AST_STATEMENT_DO_WHILE:
            ctx->loop_depth++;
            resolve_statement(stmt->left, ctx);
            ctx->loop_depth--;
            resolve_expression(stmt->right, ctx);
            break;
        case AST_STATEMENT_FOR:
            scope_push(ctx);
            if (stmt->left) {
                if (stmt->left->type == AST_DECLARATION) {
                    resolve_declaration(stmt->left, ctx);
                } else {
                    resolve_statement(stmt->left, ctx);
                }
            }
            if (stmt->right) {
                resolve_expression(stmt->right, ctx);
            }
            ctx->loop_depth++;
            resolve_statement(stmt->fourth, ctx);
            ctx->loop_depth--;
            if (stmt->third) {
                resolve_expression(stmt->third, ctx);
            }
            scope_pop(ctx);
            break;
        case AST_STATEMENT_BREAK:
            if (ctx->loop_depth <= 0) {
                SEMANTIC_ERROR("Semantic Error: 'break' used outside of a loop");
            }
            break;
        case AST_STATEMENT_CONTINUE:
            if (ctx->loop_depth <= 0) {
                SEMANTIC_ERROR("Semantic Error: 'continue' used outside of a loop");
            }
            break;
        default:
            SEMANTIC_ERROR("Semantic Error: unexpected node type in statement");
    }
}

static void resolve_expression(ASTNode *expr, ResolveContext *ctx) {
    if (!expr) return;

    switch (expr->type) {
        case AST_EXPRESSION_ASSIGNMENT:
            if (!expr->left || expr->left->type != AST_EXPRESSION_VARIABLE) {
                SEMANTIC_ERROR("Semantic Error: invalid lvalue in assignment");
            }
            resolve_expression(expr->left, ctx);
            resolve_expression(expr->right, ctx);
            break;
        case AST_EXPRESSION_VARIABLE: {
            if (!expr->value) {
                SEMANTIC_ERROR("Semantic Error: unnamed variable usage");
            }
            char *resolved = scope_lookup(ctx, expr->value);
            if (!resolved) {
                SEMANTIC_ERROR("Semantic Error: use of undeclared variable '%s'", expr->value);
            }
            set_node_value(expr, resolved);
            break;
        }
        case AST_EXPRESSION_NEGATE:
        case AST_EXPRESSION_COMPLEMENT:
        case AST_EXPRESSION_NOT:
            resolve_expression(expr->left, ctx);
            break;
        case AST_EXPRESSION_ADD:
        case AST_EXPRESSION_SUBTRACT:
        case AST_EXPRESSION_MULTIPLY:
        case AST_EXPRESSION_DIVIDE:
        case AST_EXPRESSION_REMAINDER:
        case AST_EXPRESSION_EQUAL:
        case AST_EXPRESSION_NOT_EQUAL:
        case AST_EXPRESSION_LESS_THAN:
        case AST_EXPRESSION_LESS_EQUAL:
        case AST_EXPRESSION_GREATER_THAN:
        case AST_EXPRESSION_GREATER_EQUAL:
        case AST_EXPRESSION_LOGICAL_AND:
        case AST_EXPRESSION_LOGICAL_OR:
            resolve_expression(expr->left, ctx);
            resolve_expression(expr->right, ctx);
            break;
        case AST_EXPRESSION_CONDITIONAL:
            resolve_expression(expr->left, ctx);
            resolve_expression(expr->right, ctx);
            resolve_expression(expr->third, ctx);
            break;
        case AST_EXPRESSION_CONSTANT:
            break;
        default:
            SEMANTIC_ERROR("Semantic Error: unexpected node type in expression");
    }
}

static void resolve_block_items(ASTNode *item, ResolveContext *ctx) {
    for (ASTNode *curr = item; curr; curr = curr->right) {
        if (!curr || curr->type != AST_BLOCK_ITEM) {
            SEMANTIC_ERROR("Semantic Error: invalid block item");
        }
        ASTNode *content = curr->left;
        if (!content) continue;
        if (content->type == AST_DECLARATION) {
            resolve_declaration(content, ctx);
        } else {
            resolve_statement(content, ctx);
        }
    }
}

void resolve_variables(ASTNode *program) {
    if (!program || program->type != AST_PROGRAM) {
        SEMANTIC_ERROR("Semantic Error: expected program node");
    }

    ASTNode *function = program->left;
    if (!function || function->type != AST_FUNCTION) {
        SEMANTIC_ERROR("Semantic Error: expected function definition");
    }

    ResolveContext ctx = {0};
    scope_push(&ctx); // function scope
    resolve_block_items(function->left, &ctx);
    scope_pop(&ctx);
}
