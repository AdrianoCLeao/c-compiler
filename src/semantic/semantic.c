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

typedef struct {
    char **names;
    char **resolved;
    size_t count;
    size_t capacity;
    int next_unique;
} VarMap;

static void var_map_init(VarMap *map) {
    map->names = NULL;
    map->resolved = NULL;
    map->count = 0;
    map->capacity = 0;
    map->next_unique = 0;
}

static void var_map_free(VarMap *map) {
    if (!map) return;
    for (size_t i = 0; i < map->count; i++) {
        free(map->names[i]);
        // Do not free map->resolved[i]; the AST owns these strings.
    }
    free(map->names);
    free(map->resolved);
}

static void var_map_ensure_capacity(VarMap *map) {
    if (map->count < map->capacity) return;
    size_t new_cap = map->capacity ? map->capacity * 2 : 8;
    char **new_names = (char **)realloc(map->names, new_cap * sizeof(char *));
    char **new_resolved = (char **)realloc(map->resolved, new_cap * sizeof(char *));
    if (!new_names || !new_resolved) {
        free(new_names);
        free(new_resolved);
        SEMANTIC_ERROR("Out of memory while resolving variables");
    }
    map->names = new_names;
    map->resolved = new_resolved;
    map->capacity = new_cap;
}

static char *var_map_lookup(const VarMap *map, const char *name) {
    for (size_t i = 0; i < map->count; i++) {
        if (strcmp(map->names[i], name) == 0) {
            return map->resolved[i];
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

static void var_map_add(VarMap *map, const char *name, char *resolved) {
    if (var_map_lookup(map, name) != NULL) {
        SEMANTIC_ERROR("Semantic Error: redeclaration of '%s'", name);
    }
    var_map_ensure_capacity(map);
    map->names[map->count] = strdup(name);
    if (!map->names[map->count]) {
        SEMANTIC_ERROR("Out of memory while storing variable name");
    }
    map->resolved[map->count] = resolved;
    map->count++;
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

static void resolve_expression(ASTNode *expr, VarMap *map);

static void resolve_statement(ASTNode *stmt, VarMap *map) {
    if (!stmt) return;
    switch (stmt->type) {
        case AST_STATEMENT_RETURN:
        case AST_STATEMENT_EXPRESSION:
            resolve_expression(stmt->left, map);
            break;
        case AST_STATEMENT_NULL:
            break;
        case AST_STATEMENT_IF:
            resolve_expression(stmt->left, map);
            resolve_statement(stmt->right, map);
            resolve_statement(stmt->third, map);
            break;
        default:
            SEMANTIC_ERROR("Semantic Error: unexpected node type in statement");
    }
}

static void resolve_declaration(ASTNode *decl, VarMap *map) {
    if (!decl || decl->type != AST_DECLARATION) return;
    if (!decl->value) {
        SEMANTIC_ERROR("Semantic Error: declaration missing identifier");
    }

    char *resolved = make_unique_name(decl->value, map->next_unique++);
    var_map_add(map, decl->value, resolved);

    set_node_value_transfer(decl, resolved);

    if (decl->left) {
        resolve_expression(decl->left, map);
    }
}

static void resolve_expression(ASTNode *expr, VarMap *map) {
    if (!expr) return;

    switch (expr->type) {
        case AST_EXPRESSION_ASSIGNMENT:
            if (!expr->left || expr->left->type != AST_EXPRESSION_VARIABLE) {
                SEMANTIC_ERROR("Semantic Error: invalid lvalue in assignment");
            }
            resolve_expression(expr->left, map);
            resolve_expression(expr->right, map);
            break;
        case AST_EXPRESSION_VARIABLE: {
            if (!expr->value) {
                SEMANTIC_ERROR("Semantic Error: unnamed variable usage");
            }
            char *resolved = var_map_lookup(map, expr->value);
            if (!resolved) {
                SEMANTIC_ERROR("Semantic Error: use of undeclared variable '%s'", expr->value);
            }
            set_node_value(expr, resolved);
            break;
        }
        case AST_EXPRESSION_NEGATE:
        case AST_EXPRESSION_COMPLEMENT:
        case AST_EXPRESSION_NOT:
            resolve_expression(expr->left, map);
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
            resolve_expression(expr->left, map);
            resolve_expression(expr->right, map);
            break;
        case AST_EXPRESSION_CONDITIONAL:
            resolve_expression(expr->left, map);
            resolve_expression(expr->right, map);
            resolve_expression(expr->third, map);
            break;
        case AST_EXPRESSION_CONSTANT:
            break;
        default:
            SEMANTIC_ERROR("Semantic Error: unexpected node type in expression");
    }
}

static void resolve_block_items(ASTNode *item, VarMap *map) {
    for (ASTNode *curr = item; curr; curr = curr->right) {
        if (!curr || curr->type != AST_BLOCK_ITEM) {
            SEMANTIC_ERROR("Semantic Error: invalid block item");
        }
        ASTNode *content = curr->left;
        if (!content) continue;
        if (content->type == AST_DECLARATION) {
            resolve_declaration(content, map);
        } else {
            resolve_statement(content, map);
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

    VarMap map;
    var_map_init(&map);
    resolve_block_items(function->left, &map);
    var_map_free(&map);
}
