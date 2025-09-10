#include "../../include/parser/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/util/diag.h"

static ASTNode *create_ast_node(ASTNodeType type, char *value, ASTNode *left, ASTNode *right) {
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = left;
    node->right = right;
    return node;
}

static void consume(Parser *parser, LexTokenType expected_type) {
    if (parser->current_token.type != expected_type) {
        int line = 0, col = 0;
        compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
        fprintf(stderr,
                "Syntax Error at %d:%d: Expected %s but got %s ('%s')\n",
                line, col,
                token_type_name(expected_type),
                token_type_name(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "");
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
        int line = 0, col = 0;
        compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
        fprintf(stderr, "Syntax Error at %d:%d: Expected function name, got %s ('%s')\n",
                line, col,
                token_type_name(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "");
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

static int precedence(LexTokenType t) {
    switch (t) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            return 20;
        case TOKEN_PLUS:
        case TOKEN_NEGATION: // binary minus
            return 10;
        default:
            return -1;
    }
}

static ASTNodeType binop_node_type(LexTokenType t) {
    switch (t) {
        case TOKEN_PLUS: return AST_EXPRESSION_ADD;
        case TOKEN_NEGATION: return AST_EXPRESSION_SUBTRACT;
        case TOKEN_STAR: return AST_EXPRESSION_MULTIPLY;
        case TOKEN_SLASH: return AST_EXPRESSION_DIVIDE;
        case TOKEN_PERCENT: return AST_EXPRESSION_REMAINDER;
        default: return AST_EXPRESSION_ADD; // unreachable for non-binops
    }
}

static ASTNode *parse_factor(Parser *parser);
static ASTNode *parse_binary_expr(Parser *parser, int min_prec) {
    ASTNode *left = parse_factor(parser);

    for (;;) {
        LexTokenType op_tok = parser->current_token.type;
        int prec = precedence(op_tok);
        if (prec < min_prec) break;

        consume(parser, op_tok);

        int next_min_prec = prec + 1;
        ASTNode *right = parse_binary_expr(parser, next_min_prec);

        left = create_ast_node(binop_node_type(op_tok), NULL, left, right);
    }

    return left;
}

static ASTNode *parse_factor(Parser *parser) {
    if (parser->current_token.type == TOKEN_CONSTANT) {
        ASTNode *constant = create_ast_node(AST_EXPRESSION_CONSTANT, parser->current_token.value, NULL, NULL);
        consume(parser, TOKEN_CONSTANT);
        return constant;
    } else if (parser->current_token.type == TOKEN_NEGATION || parser->current_token.type == TOKEN_TILDE) {
        LexTokenType op = parser->current_token.type;
        consume(parser, op);
        ASTNode *inner_expr = parse_factor(parser);
        return create_ast_node(
            (op == TOKEN_NEGATION) ? AST_EXPRESSION_NEGATE : AST_EXPRESSION_COMPLEMENT,
            NULL,
            inner_expr,
            NULL
        );
    } else if (parser->current_token.type == TOKEN_OPEN_PAREN) {
        consume(parser, TOKEN_OPEN_PAREN);
        ASTNode *inner_expr = parse_binary_expr(parser, 0);
        consume(parser, TOKEN_CLOSE_PAREN);
        return inner_expr;
    } else {
        int line = 0, col = 0;
        compute_line_col(parser->lexer->input, parser->current_token.start, &line, &col);
        fprintf(stderr, "Syntax Error at %d:%d: Expected an expression, got %s ('%s')\n",
                line, col,
                token_type_name(parser->current_token.type),
                parser->current_token.value ? parser->current_token.value : "");
        exit(1);
    }
}

ASTNode *parse_expression(Parser *parser) {
    return parse_binary_expr(parser, 0);
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
        case AST_EXPRESSION_NEGATE:
            printf("Negate\n");
            break;
        case AST_EXPRESSION_COMPLEMENT:
            printf("Complement\n");
            break;
        case AST_EXPRESSION_ADD:
            printf("Add\n");
            break;
        case AST_EXPRESSION_SUBTRACT:
            printf("Subtract\n");
            break;
        case AST_EXPRESSION_MULTIPLY:
            printf("Multiply\n");
            break;
        case AST_EXPRESSION_DIVIDE:
            printf("Divide\n");
            break;
        case AST_EXPRESSION_REMAINDER:
            printf("Remainder\n");
            break;
        default:
            printf("Unknown Node\n");
    }

    print_ast(node->left, depth + 1);
    print_ast(node->right, depth + 1);
}
