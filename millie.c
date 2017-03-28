#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

#include "platform.c"
#include "cityhash.c"
#include "string.c"
#include "errors.c"
#include "symboltable.c"
#include "lexer.c"
#include "ast.c"
#include "parser.c"
#include "typecheck.c"

/*
 * Driver
 */

void PrintTypeExpression(struct TypeExp *type);
void PrintTypeExpression(struct TypeExp *type)
{
    switch(type->type) {
    case TYPEEXP_VARIABLE:
    case TYPEEXP_OPERATOR:
    case TYPEEXP_INVALID:
        printf("???\n");
    }
}

void PrintErrors(const char *fname, struct MillieTokens *tokens,
                 struct Errors *errors);
void PrintTokens(struct MillieTokens *tokens);

void PrintErrors(const char *fname, struct MillieTokens *tokens,
                 struct Errors *errors)
{
    struct ErrorReport *error = FirstError(errors);
    while(error) {
        unsigned int start_line, start_col, end_line, end_col;

        GetLineColumnForPosition(
            tokens,
            error->start_pos,
            &start_line,
            &start_col
        );
        GetLineColumnForPosition(
            tokens,
            error->end_pos,
            &end_line,
            &end_col
        );

        printf(
            "%s:%d,%d: error: %s\n",
            fname,
            start_line,
            start_col,
            MStringData(error->message)
        );

        struct MString *line = ExtractLine(tokens, start_line);
        if (end_line != start_line) {
            end_col = MStringLength(line);
        }
        printf("%s\n", MStringData(line));
        for(unsigned int i = 1; i <= MStringLength(line); i++) {
            if (i < start_col) {
                printf(" ");
            } else if (i == start_col) {
                printf("^");
            } else if (i < end_col) {
                printf("~");
            } else {
                printf(" ");
            }
        }
        printf("\n");
        MStringFree(&line);

        error = error->next;
    }
}

void PrintTokens(struct MillieTokens *tokens)
{
    for(unsigned int i = 0; i < tokens->token_array->item_count; i++) {
        struct MillieToken token;
        token = *(struct MillieToken *)(ArrayListIndex(tokens->token_array, i));

        struct MString *substr = MStringCreateN(
            MStringData(tokens->buffer) + token.start,
            token.length
        );
        printf("%03d: %s\n", token.type, MStringData(substr));
        MStringFree(&substr);
    }
}

void PrintIndent(int indent);
void PrintIndent(int indent)
{
    for(int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void PrintTree(struct SymbolTable *table, struct MillieTokens *tokens,
               struct Expression *expression, int indent);
void PrintTree(struct SymbolTable *table, struct MillieTokens *tokens,
               struct Expression *expression, int indent)
{
    switch(expression->type) {
    case EXP_LAMBDA:
        {
            struct MString *id = FindSymbolKey(table, expression->lambda_id);
            PrintIndent(indent); printf("lambda %s =>\n", MStringData(id));
            PrintTree(table, tokens, expression->lambda_body, indent+1);
            MStringFree(&id);
        }
        break;

    case EXP_IDENTIFIER:
        {
            struct MString *id = FindSymbolKey(table, expression->identifier_id);
            PrintIndent(indent); printf("id %s\n", MStringData(id));
            MStringFree(&id);
        }
        break;

    case EXP_APPLY:
        {
            PrintIndent(indent); printf("apply\n");
            PrintTree(table, tokens, expression->apply_function, indent+1);
            PrintTree(table, tokens, expression->apply_argument, indent+1);
        }
        break;

    case EXP_LET:
        {
            struct MString *id = FindSymbolKey(table, expression->let_id);
            PrintIndent(indent); printf("let %s = \n", MStringData(id));
            PrintTree(table, tokens, expression->let_value, indent+1);
            PrintIndent(indent); printf("in\n");
            PrintTree(table, tokens, expression->let_body, indent+1);
            MStringFree(&id);
        }
        break;
    case EXP_LETREC:
        {
            struct MString *id = FindSymbolKey(table, expression->let_id);
            PrintIndent(indent); printf("let rec %s = \n", MStringData(id));
            PrintTree(table, tokens, expression->let_value, indent+1);
            PrintIndent(indent); printf("in\n");
            PrintTree(table, tokens, expression->let_body, indent+1);
            MStringFree(&id);
        }
        break;

    case EXP_INTEGER_CONSTANT:
        {
            PrintIndent(indent);
            printf("literal %llu\n", expression->literal_value);
        }
        break;

    case EXP_TRUE:
        {
            PrintIndent(indent); printf("true\n");
        }
        break;

    case EXP_FALSE:
        {
            PrintIndent(indent); printf("false\n");
        }
        break;

    case EXP_IF:
        {
            PrintIndent(indent); printf("if\n");
            PrintTree(table, tokens, expression->if_test, indent+1);
            PrintIndent(indent); printf("then\n");
            PrintTree(table, tokens, expression->if_then, indent+1);
            PrintIndent(indent); printf("else\n");
            PrintTree(table, tokens, expression->if_else, indent+1);
        }
        break;

    case EXP_BINARY:
        {
            uint32_t bin_tok = expression->binary_left->end_token + 1;
            struct MString *operator = ExtractToken(tokens, bin_tok);
            PrintIndent(indent); printf("binary %s\n", MStringData(operator));
            PrintTree(table, tokens, expression->binary_left, indent+1);
            PrintTree(table, tokens, expression->binary_right, indent+1);
            MStringFree(&operator);
        }
        break;

    case EXP_UNARY:
        {
            struct MString *operator;
            operator = ExtractToken(tokens, expression->start_token);
            PrintIndent(indent); printf("unary %s\n", MStringData(operator));
            PrintTree(table, tokens, expression->unary_arg, indent+1);
            MStringFree(&operator);
        }
        break;

    case EXP_ERROR:
        PrintIndent(indent); printf("ERROR");
        break;

    case EXP_INVALID:
        PrintIndent(indent); printf("???\n");
        break;
    }
}

int main()
{
    struct MString *buffer = MStringCreate(
        "let rec factorial =\n"
        "  fn n => if n = 0 then 1 else n * factorial (n + -1)\n"
        "in factorial 5\n"
    );

    struct Errors *errors;
    struct MillieTokens *tokens = LexBuffer(buffer, &errors);
    if (errors) {
        PrintErrors("test", tokens, errors);
        return 1;
    }

    // PrintTokens(tokens);

    struct Arena *arena = MakeFreshArena();
    struct SymbolTable *symbol_table = SymbolTableCreate();
    struct Expression *expression;
    expression = ParseExpression(arena, tokens, symbol_table, &errors);
    if (errors) {
        PrintErrors("test", tokens, errors);
        return 1;
    }

    PrintTree(symbol_table, tokens, expression, 0);
    printf("\n");

    struct TypeExp *type = TypeExpression(arena, tokens, expression, &errors);
    if (errors) {
        PrintErrors("test", tokens, errors);
        return 1;
    }
    PrintTypeExpression(type);

    printf("Arena: %lu bytes used\n", ArenaAllocated(arena));
    printf("Size of expression is %lu bytes\n", sizeof(struct Expression));
    FreeArena(&arena);

    /* struct CheckContext *context = MakeNewCheckContext(arena); */
    /* struct Expression *node = ParseExpression(arena, tokens); */
    /* struct TypeExp *type = Analyze(context, node, NULL, NULL); */
    /* PrintTypeExpression(type); */
    /* FreeArena(&arena); */

    return 0;
}
