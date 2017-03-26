#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NORETURN  __attribute__((noreturn))

/*
 * Assertion and failure
 */
void Fail(char *message);

/*
 * Memory allocation
 */
struct Arena;

struct Arena *MakeFreshArena(void);
void FreeArena(struct Arena **arena);
void *ArenaAllocate(struct Arena *arena, size_t size);

/*
 * Hash functions
 */
uint32_t CityHash32(const char *s, size_t len);

/*
 * String functions
 */
struct MString;

struct MString *MStringCreate(char *str);
struct MString *MStringCreateN(const char *str, unsigned int length);
void MStringFree(struct MString **string_ptr);
struct MString *MStringCopy(struct MString *string);
struct MString *MStringCatV(int count, ...);
struct MString *MStringCat(struct MString *first, struct MString *second);
struct MString *MStringPrintF(const char *format, ...);
struct MString *MStringPrintFV(const char *format, va_list args);
unsigned int MStringLength(struct MString *string);
const char *MStringData(struct MString *string);
unsigned int MStringHash32(struct MString *string);
bool MStringEquals(struct MString *a, struct MString *b);

/*
 * List functions
 */
struct ArrayList {
    unsigned int item_count;
    unsigned int capacity;
    size_t item_size;
    void *buffer;
};

struct ArrayList *ArrayListCreate(size_t item_size,
                                  unsigned int capacity);
void ArrayListFree(struct ArrayList **array_ptr);
void *ArrayListIndex(struct ArrayList *array, unsigned int index);
unsigned int ArrayListAdd(struct ArrayList *array, void *item);


/*
 * Error functions
 */
struct ErrorReport {
    struct ErrorReport *next;
    struct MString *message;
    unsigned int start_pos;
    unsigned int end_pos;
};

struct Errors;

void AddError(struct Errors **errors_ptr, unsigned int start_pos,
              unsigned int end_pos, struct MString *message);
void AddErrorF(struct Errors **errors_ptr, unsigned int start_pos,
               unsigned int end_pos, const char *format, ...);
void FreeErrors(struct Errors **errors_ptr);
struct ErrorReport *FirstError(struct Errors *errors);

/*
 * Lexer
 */
typedef enum {
    TOK_EOF = 0,
    TOK_ID,
    TOK_INT_LITERAL,
    TOK_TRUE,
    TOK_FALSE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_FN,
    TOK_IF,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_EQUALS,
    TOK_ARROW,
    TOK_LET,
    TOK_REC,
} MILLIE_TOKEN;

struct MillieToken {
    MILLIE_TOKEN type;
    unsigned int start;
    unsigned int length;
};

struct MillieTokens {
    struct ArrayList *token_array; // of MillieToken
    struct ArrayList *line_array;  // of int
    struct MString *buffer;
};

void TokensFree(struct MillieTokens **tokens_ptr);
struct MillieTokens *LexBuffer(struct MString *buffer, struct Errors **errors);
void GetLineColumnForPosition(struct MillieTokens *tokens, unsigned int position,
                              unsigned int *line, unsigned int *col);
struct MString *ExtractLine(struct MillieTokens *tokens, unsigned int line);

/*
 * Symbol Tables
 */
typedef uint32_t Symbol;
#define INVALID_SYMBOL (0)
struct SymbolTable;

struct SymbolTable *SymbolTableCreate(void);
void SymbolTableFree(struct SymbolTable **table_ptr);
Symbol FindOrCreateSymbol(struct SymbolTable *table, struct MString *key);

/*
 * AST
 */
typedef enum {
    EXP_INVALID = 0,
    EXP_LAMBDA = 1,
    EXP_IDENTIFIER = 2,
    EXP_APPLY = 3,
    EXP_LET = 4,
    EXP_LETREC = 5,
    EXP_INTEGER_CONSTANT = 6,
    EXP_TRUE = 7,
    EXP_FALSE = 8,
} ExpressionType;

struct Expression {
    ExpressionType type;
    Symbol id;
    union
    {
        struct Expression *apply_function;
        struct Expression *lambda_body;
        struct Expression *let_value;
    };
    union
    {
        struct Expression *apply_argument;
        struct Expression *let_body;
    };
};

struct Expression *MakeLambda(struct Arena *arena, Symbol variable,
                              struct Expression *body);
struct Expression *MakeIdentifier(struct Arena *arena, Symbol id);
struct Expression *MakeApply(struct Arena *arena, struct Expression *func_expr,
                             struct Expression *arg_expr);
struct Expression *MakeLet(struct Arena *arena, Symbol variable,
                           struct Expression *value, struct Expression *body);
struct Expression *MakeLetRec(struct Arena *arena, Symbol variable,
                              struct Expression *value, struct Expression *body);


#define PLATFORM_INCLUDED
