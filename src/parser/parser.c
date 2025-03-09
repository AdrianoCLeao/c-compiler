#include "../../include/parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASTNode *create_ast_node(ASTNodeType type, char *value, ASTNode *left, ASTNode *right) {
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    return node;
}

static void consume(Parser *parser, TokenType expected_type) {
    if (parser->current_token.type != expected_type) {
        fprintf(stderr, "Syntax Error: Expected token %d but got %d\n", expected_type, parser->current_token.type);
        exit(1);
    }
    free_token(parser->current_token);
    parser->current_token = lexer_next_token(parser->lexer);
}

void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->current_token = lexer_next_token(lexer);
}

ASTNode *parse_program(Parser *parser) {
    return create_ast_node(AST_PROGRAM, NULL, parse_function(parser), NULL);
}

ASTNode *parse_function(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_INT); 
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Syntax Error: Expected function name\n");
        exit(1);
    }

    ASTNode *func_name = create_ast_node(AST_EXPRESSION_IDENTIFIER, parser->current_token.value, NULL, NULL);
    consume(parser, TOKEN_IDENTIFIER);

    consume(parser, TOKEN_OPEN_PAREN);
    consume(parser, TOKEN_KEYWORD_VOID); 
    consume(parser, TOKEN_CLOSE_PAREN);  
    consume(parser, TOKEN_OPEN_BRACE);  

    ASTNode *body = parse_statement(parser);

    consume(parser, TOKEN_CLOSE_BRACE);

    return create_ast_node(AST_FUNCTION, "main", func_name, body);
}

ASTNode *parse_statement(Parser *parser) {
    consume(parser, TOKEN_KEYWORD_RETURN); 
    ASTNode *expr = parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON); 
    return create_ast_node(AST_STATEMENT_RETURN, NULL, expr, NULL);
}

ASTNode *parse_expression(Parser *parser) {
    if (parser->current_token.type == TOKEN_CONSTANT) {
        ASTNode *constant = create_ast_node(AST_EXPRESSION_CONSTANT, parser->current_token.value, NULL, NULL);
        consume(parser, TOKEN_CONSTANT);
        return constant;
    } else {
        fprintf(stderr, "Syntax Error: Expected an expression\n");
        exit(1);
    }
}

void free_ast(ASTNode *node) {
    if (!node) return;
    free(node->value);
    free_ast(node->left);
    free_ast(node->right);
    free(node);
}

void print_ast(ASTNode *node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    switch (node->type) {
        case AST_PROGRAM:
            printf("Program\n");
            break;
        case AST_FUNCTION:
            printf("Function: %s\n", node->value);
            break;
        case AST_STATEMENT_RETURN:
            printf("Return\n");
            break;
        case AST_EXPRESSION_CONSTANT:
            printf("Constant: %s\n", node->value);
            break;
        case AST_EXPRESSION_IDENTIFIER:
            printf("Identifier: %s\n", node->value);
            break;
        default:
            printf("Unknown Node\n");
    }

    print_ast(node->left, depth + 1);
    print_ast(node->right, depth + 1);
}
