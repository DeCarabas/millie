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

static void PrintErrors(const char *fname, struct MillieTokens *tokens,
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

        fprintf(
            stderr,
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
        fprintf(stderr, "%s\n", MStringData(line));
        for(unsigned int i = 1; i <= MStringLength(line); i++) {
            if (i < start_col) {
                fprintf(stderr, " ");
            } else if (i == start_col) {
                fprintf(stderr, "^");
            } else if (i < end_col) {
                fprintf(stderr, "~");
            } else {
                fprintf(stderr, " ");
            }
        }
        fprintf(stderr, "\n");
        MStringFree(&line);

        error = error->next;
    }
}


static struct MString *ReadFile(const char *filename)
{
    struct stat filestat;
    if (stat(filename, &filestat) != 0) {
        fprintf(stderr, "Failed to stat file\n");
        return NULL;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file\n");
        return NULL;
    }

    char *buff = malloc(filestat.st_size + 1);
    size_t readcount = fread(buff, 1, filestat.st_size, file);
    fclose(file);

    if (readcount != (size_t)filestat.st_size) {
        fprintf(
            stderr,
            "Failed to read the file: read %lu expected %lld\n",
            readcount,
            filestat.st_size
        );
        return NULL;
    }

    // TODO: Here is a case where I could avoid double-allocating.
    struct MString *buffer = MStringCreateN(buff, readcount);
    free(buff);

    return buffer;
}

static void _print_usage()
{
    printf(
        "Usage: millie [switches] <input file>\n"
        "  --print-type  -t  Print the type of the expression in the input\n"
        "                    file to stdout, instead of evaluating.\n"
        "  --verbose     -v  Print various other things to stdout.\n"
    );
}

int main(int argc, const char *argv[])
{
    const char *fname = NULL;
    bool print_type = false;
    bool verbose = false;
    for(int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg[0] == '-') {
            if (strcmp(arg, "--verbose") == 0) {
                verbose = true;
            } else if (strcmp(arg, "--print-type") == 0) {
                print_type = true;
            } else if (strcmp(arg, "--help") == 0) {
                _print_usage();
                return 0;
            } else if (arg[1] == '-') {
                fprintf(stderr, "Unknown switch '%s'\n\n", arg);
                return -1;
            } else {
                arg++;
                while(*arg) {
                    switch(*arg) {
                    case 't': print_type = true; break;
                    case 'v': verbose = true; break;
                    case 'h':
                    case '?':
                        _print_usage();
                        return 0;
                    default:
                        fprintf(stderr, "Unknown switch '%c'\n\n", *arg);
                        return -1;
                    }
                    arg++;
                }
            }
        } else {
            if (fname) {
                fprintf(stderr, "More than one input file unsupported.\n");
                return -1;
            } else {
                fname = arg;
            }
        }
    }

    if (fname == NULL) {
        fprintf(stderr, "No input file specified.\n");
        return -1;
    }

    struct MString *buffer = ReadFile(fname);
    if (!buffer) { return -1; }

    struct Errors *errors;
    struct MillieTokens *tokens = LexBuffer(buffer, &errors);
    if (errors) {
        PrintErrors(fname, tokens, errors);
        return 1;
    }

    struct Arena *arena = MakeFreshArena();
    struct SymbolTable *symbol_table = SymbolTableCreate();
    struct Expression *expression;
    expression = ParseExpression(arena, tokens, symbol_table, &errors);
    if (errors) {
        PrintErrors(fname, tokens, errors);
        return 1;
    }

    struct TypeExp *type = GetExpressionType(arena, tokens, expression, &errors);
    if (errors) {
        PrintErrors(fname, tokens, errors);
        return 1;
    }

    if (print_type) {
        struct MString *typeexp = FormatTypeExpression(type);
        printf("%s\n", MStringData(typeexp));
        MStringFree(&typeexp);
    }

    if (verbose) {
        fprintf(stderr, "Arena: %lu bytes used\n", ArenaAllocated(arena));
        fprintf(stderr, "Size of expression is %lu bytes\n", sizeof(struct Expression));
        fprintf(stderr, "Size of type exp is %lu bytes\n", sizeof(struct TypeExp));
    }

    FreeArena(&arena);

    return 0;
}
