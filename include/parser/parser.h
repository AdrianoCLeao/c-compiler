#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "../lexer/lexer.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_BLOCK_ITEM,
    AST_DECLARATION,
    AST_STATEMENT_RETURN,
    AST_STATEMENT_EXPRESSION,
    AST_STATEMENT_NULL,
    AST_STATEMENT_IF,
    AST_STATEMENT_COMPOUND,
    AST_STATEMENT_WHILE,
    AST_STATEMENT_DO_WHILE,
    AST_STATEMENT_FOR,
    AST_STATEMENT_BREAK,
    AST_STATEMENT_CONTINUE,
    AST_EXPRESSION_CONSTANT,
    AST_EXPRESSION_VARIABLE,
    AST_EXPRESSION_ASSIGNMENT,
    AST_EXPRESSION_CONDITIONAL,
    AST_EXPRESSION_NEGATE,
    AST_EXPRESSION_COMPLEMENT,
    AST_EXPRESSION_NOT,
    AST_EXPRESSION_ADD,
    AST_EXPRESSION_SUBTRACT,
    AST_EXPRESSION_MULTIPLY,
    AST_EXPRESSION_DIVIDE,
    AST_EXPRESSION_REMAINDER,
    AST_EXPRESSION_EQUAL,
    AST_EXPRESSION_NOT_EQUAL,
    AST_EXPRESSION_LESS_THAN,
    AST_EXPRESSION_LESS_EQUAL,
    AST_EXPRESSION_GREATER_THAN,
    AST_EXPRESSION_GREATER_EQUAL,
    AST_EXPRESSION_LOGICAL_AND,
    AST_EXPRESSION_LOGICAL_OR,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *third;
    struct ASTNode *fourth;
    char *value;
    bool owns_value;
} ASTNode;

typedef struct {
    Lexer *lexer;
    Token current_token;
} Parser;

void parser_init(Parser *parser, Lexer *lexer);
ASTNode *parse_program(Parser *parser);
void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int depth);

#endif 
