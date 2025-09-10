#ifndef PARSER_H
#define PARSER_H

#include "../lexer/lexer.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_STATEMENT_RETURN,
    AST_EXPRESSION_CONSTANT,
    AST_EXPRESSION_IDENTIFIER,
    AST_EXPRESSION_NEGATE,
    AST_EXPRESSION_COMPLEMENT,
    AST_EXPRESSION_ADD,
    AST_EXPRESSION_SUBTRACT,
    AST_EXPRESSION_MULTIPLY,
    AST_EXPRESSION_DIVIDE,
    AST_EXPRESSION_REMAINDER,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    struct ASTNode *left;
    struct ASTNode *right;
    char *value;
} ASTNode;

typedef struct {
    Lexer *lexer;
    Token current_token;
} Parser;

void parser_init(Parser *parser, Lexer *lexer);
ASTNode *parse_program(Parser *parser);
ASTNode *parse_function(Parser *parser);
ASTNode *parse_statement(Parser *parser);
ASTNode *parse_expression(Parser *parser);
void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int depth);

#endif 
