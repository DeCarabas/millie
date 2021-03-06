// ----------------------------------------------------------------------------
// This is the giant uber-include file for the Millie implementation.
// Everything needed cross-module is in here.
// ----------------------------------------------------------------------------
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NORETURN  __attribute__((noreturn))

// ----------------------------------------------------------------------------
// Assertion and Failure
// ----------------------------------------------------------------------------

void Fail(char *message);


// ----------------------------------------------------------------------------
// Memory Allocation
// ----------------------------------------------------------------------------

struct Arena;

struct Arena *MakeFreshArena(void);
void FreeArena(struct Arena **arena);
void *ArenaAllocate(struct Arena *arena, size_t size);
size_t ArenaAllocated(struct Arena *arena);


// ----------------------------------------------------------------------------
// Hash Functions
// ----------------------------------------------------------------------------

uint32_t CityHash32(const char *s, size_t len);


// ----------------------------------------------------------------------------
// Strings
// ----------------------------------------------------------------------------

struct MString;
struct MStringStatic {
    int _r0[2];
    void *_r1[1];
};

struct MString *MStringCreate(const char *str);
struct MString *MStringCreateN(const char *str, unsigned int length);
struct MString *MStringCreateStatic(const char *str, struct MStringStatic *blk);
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


// ----------------------------------------------------------------------------
// Array List
// ----------------------------------------------------------------------------

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


// ----------------------------------------------------------------------------
// Errors
// ----------------------------------------------------------------------------

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
void AddErrorFV(struct Errors **errors_ptr, unsigned int start_pos,
                unsigned int end_pos, const char *format, va_list args);
void FreeErrors(struct Errors **errors_ptr);
struct ErrorReport *FirstError(struct Errors *errors);


// ----------------------------------------------------------------------------
// Lexer
// ----------------------------------------------------------------------------

typedef enum {
    TOK_EOF = 0,

    TOK_FIRST_PRIMARY,
    TOK_ID = TOK_FIRST_PRIMARY,
    TOK_INT_LITERAL,
    TOK_TRUE,
    TOK_FALSE,
    TOK_LPAREN,
    TOK_LAST_PRIMARY = TOK_LPAREN,

    TOK_PLUS,  // + is here for unary +.
    TOK_MINUS, // - is here for unary -.
    TOK_RPAREN,
    TOK_FN,
    TOK_IF,
    TOK_IN,
    TOK_STAR,
    TOK_SLASH,
    TOK_EQUALS,
    TOK_ARROW,
    TOK_LET,
    TOK_REC,
    TOK_THEN,
    TOK_ELSE,
    TOK_COMMA,
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
struct MString *ExtractToken(struct MillieTokens *tokens, uint32_t pos);
struct MillieToken GetToken(struct MillieTokens *tokens, uint32_t pos);
void PrintTokens(struct MillieTokens *tokens);


// ----------------------------------------------------------------------------
// Symbol Table
// ----------------------------------------------------------------------------

typedef uint32_t Symbol;
#define INVALID_SYMBOL (0)
struct SymbolTable;

struct SymbolTable *SymbolTableCreate(void);
void SymbolTableFree(struct SymbolTable **table_ptr);
Symbol FindOrCreateSymbol(struct SymbolTable *table, struct MString *key);
struct MString *FindSymbolKey(struct SymbolTable *table, Symbol symbol);


// ----------------------------------------------------------------------------
// AST
// ----------------------------------------------------------------------------

typedef enum {
    EXP_INVALID = 0,
    EXP_ERROR,
    EXP_LAMBDA,
    EXP_IDENTIFIER,
    EXP_APPLY,
    EXP_LET,
    EXP_LETREC,
    EXP_INTEGER_CONSTANT,
    EXP_TRUE,
    EXP_FALSE,
    EXP_IF,
    EXP_BINARY,
    EXP_UNARY,
    EXP_TUPLE,
    EXP_TUPLE_FINAL,
} ExpressionType;

struct Expression {
    ExpressionType type;
    union
    {
        struct
        {
            struct Expression *lambda_body;
            Symbol lambda_id;
        };
        struct
        {
            Symbol identifier_id;
        };
        struct
        {
            struct Expression *apply_function;
            struct Expression *apply_argument;
        };
        struct
        {
            Symbol let_id;
            struct Expression *let_value;
            struct Expression *let_body;
        };
        struct
        {
            uint64_t literal_value;
        };
        struct
        {
            struct Expression *if_test;
            struct Expression *if_then;
            struct Expression *if_else;
        };
        struct
        {
            MILLIE_TOKEN binary_operator;
            struct Expression *binary_left;
            struct Expression *binary_right;
        };
        struct
        {
            MILLIE_TOKEN unary_operator;
            struct Expression *unary_arg;
        };
        struct
        {
            struct Expression *tuple_first;
            struct Expression *tuple_rest;
            int tuple_length;
        };
    };
    uint32_t start_token;
    uint32_t end_token;
};

struct Expression *MakeSyntaxError(struct Arena *arena, uint32_t position);
struct Expression *MakeLambda(struct Arena *arena, uint32_t start_token,
                              Symbol variable, struct Expression *body);
struct Expression *MakeIdentifier(struct Arena *arena, uint32_t token_pos,
                                  Symbol id);
struct Expression *MakeApply(struct Arena *arena, struct Expression *func_expr,
                             struct Expression *arg_expr);
struct Expression *MakeLet(struct Arena *arena, uint32_t let_pos,
                           Symbol variable, struct Expression *value,
                           struct Expression *body);
struct Expression *MakeLetRec(struct Arena *arena, uint32_t let_pos,
                              Symbol variable, struct Expression *value,
                              struct Expression *body);
struct Expression *MakeIf(struct Arena *arena, uint32_t if_pos,
                          struct Expression *test,
                          struct Expression *then_branch,
                          struct Expression *else_branch);
struct Expression *MakeBinary(struct Arena *arena, MILLIE_TOKEN op,
                              struct Expression *left,
                              struct Expression *right);
struct Expression *MakeUnary(struct Arena *arena, uint32_t operator_pos,
                             MILLIE_TOKEN op, struct Expression *arg);
struct Expression *MakeBooleanLiteral(struct Arena *arena, uint32_t pos,
                                      bool value);
struct Expression *MakeIntegerLiteral(struct Arena *arena, uint32_t pos,
                                      uint64_t value);
struct Expression *MakeTuple(struct Arena *arena, struct Expression *first,
                             struct Expression *rest, int length);
struct Expression *MakeTupleFinal(struct Arena *arena, struct Expression *expr);
void DumpExpression(struct SymbolTable *table, struct MillieTokens *tokens,
                    struct Expression *expression);


// ----------------------------------------------------------------------------
// Parser
// ----------------------------------------------------------------------------

struct Expression *ParseExpression(struct Arena *arena,
                                   struct MillieTokens *tokens,
                                   struct SymbolTable *symbol_table,
                                   struct Errors **errors);



// ----------------------------------------------------------------------------
// Type Checker
// ----------------------------------------------------------------------------

typedef enum {
    TYPEEXP_INVALID = 0,
    TYPEEXP_ERROR,
    TYPEEXP_VARIABLE,
    TYPEEXP_GENERIC_VARIABLE,
    TYPEEXP_FUNC,
    TYPEEXP_INT,
    TYPEEXP_BOOL,
    TYPEEXP_TUPLE,
    TYPEEXP_TUPLE_FINAL,
} TypeExpType;

struct TypeExp {
    TypeExpType type;
    union
    {
        struct TypeExp *arg_first;
        struct TypeExp *func_from;
        struct TypeExp *tuple_first;
        struct TypeExp *var_instance;
    };
    union
    {
        struct TypeExp *arg_second;
        struct TypeExp *func_to;
        struct TypeExp *tuple_rest;
        struct TypeExp *var_temp_other;
    };
};

struct MString *FormatTypeExpression(struct TypeExp *type);
struct TypeExp *GetExpressionType(
    struct Arena *arena,
    struct Expression *node,
    struct MillieTokens *tokens,
    struct Errors **errors);


// ----------------------------------------------------------------------------
// Bytecode Compiler
// ----------------------------------------------------------------------------

typedef enum MILLIE_OPCODE {
#define OPCODE(name, _x, _y, _z)  OP_##name ,
#include "opcodes.inc"
#undef OPCODE
} MILLIE_OPCODE;

struct RuntimeClosure {
    uint64_t function_id;
    uint64_t slots[0];
};

struct CompiledExpression {
    uint8_t *code;
    size_t code_length;
    size_t register_count;

    // If closure_length > 0, then:
    //
    // - The closure array is a list of symbols used to tell the compiler what
    //   symbols need to be in the closure.
    //
    // - The NEW_CLOSURE opcode will allocate a new RuntimeClosure object, with
    //   closure_length slots.
    //
    // - The compiler will issue closure_length loads in order to initialize
    //   each slot of the dynamically-allocated closure.
    //
    // If closure_length == 0, then:
    //
    // - The static_closure structure is initialized with the function ID of
    //   the CompiledExpression within its module.
    //
    // - The NEW_CLOSURE opcode will put the address of static_closure into the
    //   destination register.
    //
    // - The compiler will not issue any loads.
    //
    size_t closure_length;
    union {
        Symbol *closure;
        struct RuntimeClosure static_closure;
    };

    uint8_t result_register;
};

struct Module {
    struct CompiledExpression *functions;
    int function_count;
    int function_capacity;
};

void ModuleInit(struct Module *module);
int CompileExpression(struct Expression *expression,
                      struct MillieTokens *tokens,
                      struct Errors **errors,
                      struct Module *result);

#define PLATFORM_INCLUDED
