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

/*
 * Type Variables
 */
typedef enum {
    TYPEEXP_INVALID = 0,
    TYPEEXP_VARIABLE = 1,
    TYPEEXP_OPERATOR = 2,
} TypeExpType;

struct TypeExp {
    TypeExpType type;
    int id;
    char *name;
    union
    {
        struct TypeExp *arg_first;
        struct TypeExp *func_from;
        struct TypeExp *var_instance;
    };
    union
    {
        struct TypeExp *arg_second;
        struct TypeExp *func_to;
        struct TypeExp *var_temp_other;
    };
};

struct NonGenericTypeList {
    struct NonGenericTypeList *next;
    struct TypeExp *type;
};

struct NonGenericTypeList *ExtendNonGenericTypeList(
    struct Arena *arena,
    struct TypeExp *type,
    struct NonGenericTypeList *non_generics
);
int IsTypeContainedWithin(struct TypeExp *a, struct TypeExp *b);
int IsTypeNonGeneric(struct TypeExp *a,
                     struct NonGenericTypeList *non_generics);
struct TypeExp *PruneTypeExp(struct TypeExp *type);
struct TypeExp *MakeTypeVar(struct Arena *arena);
struct TypeExp *MakeFunctionTypeExp(struct Arena *arena,
                                    struct TypeExp *from_type,
                                    struct TypeExp *to_type);
struct TypeExp *MakeFreshTypeExpCopy(struct Arena *arena,
                                     struct TypeExp *type,
                                     struct NonGenericTypeList *non_generics);
void MakeFreshTypeExpCleanup(struct TypeExp *type);
struct TypeExp *MakeFreshTypeExp(struct Arena *arena, struct TypeExp *type,
                                 struct NonGenericTypeList *non_generics);

struct NonGenericTypeList *
ExtendNonGenericTypeList(struct Arena *arena, struct TypeExp *type,
                         struct NonGenericTypeList *non_generics)
{
    struct NonGenericTypeList *new_list;
    new_list = ArenaAllocate(arena, sizeof(struct NonGenericTypeList));
    new_list->next = non_generics;
    new_list->type = type;
    return new_list;
}

int
IsTypeContainedWithin(struct TypeExp *a, struct TypeExp *b)
{
    if (a == b) { return 1; }
    if (b == NULL) { return 0; }
    if (IsTypeContainedWithin(a, b->arg_first)) { return 1; }
    if (IsTypeContainedWithin(a, b->arg_second)) { return 1; }
    return 0;
}

int
IsTypeNonGeneric(struct TypeExp *a, struct NonGenericTypeList *non_generics)
{
    while(non_generics) {
        if (IsTypeContainedWithin(a, non_generics->type)) {
            return 1;
        }
        non_generics = non_generics->next;
    }
    return 0;
}

struct TypeExp *
PruneTypeExp(struct TypeExp *type)
{
    if (!type) { return NULL; }
    while(type->type == TYPEEXP_VARIABLE && type->var_instance) {
        type = type->var_instance;
    }
    return type;
}

struct TypeExp *
MakeTypeVar(struct Arena *arena)
{
    struct TypeExp *result = ArenaAllocate(arena, sizeof(struct TypeExp));
    result->type = TYPEEXP_VARIABLE;
    return result;
}

struct TypeExp *
MakeFunctionTypeExp(struct Arena *arena, struct TypeExp *from_type,
                    struct TypeExp *to_type)
{
    struct TypeExp *result;
    result = ArenaAllocate(arena, sizeof(struct TypeExp));
    result->type = TYPEEXP_OPERATOR;
    result->name = "->";
    result->func_from = from_type;
    result->func_to = to_type;
    return result;
}

static struct TypeExp *TYPEEXP_NON_GENERIC_MARKER =
    (struct TypeExp *)(-1);

struct TypeExp *
MakeFreshTypeExpCopy(struct Arena *arena, struct TypeExp *type,
                     struct NonGenericTypeList *non_generics)
{
    type = PruneTypeExp(type);
    if (type->type == TYPEEXP_VARIABLE) {
        if (type->var_temp_other == TYPEEXP_NON_GENERIC_MARKER) {
            /* KNOWN NOT GENERIC, JUST RETURN. */
            return type;
        } else if (type->var_temp_other) {
            /* KNOWN GENERIC, ALREADY COPIED, QUICK OUT. */
            return type->var_temp_other;
        } else if (IsTypeNonGeneric(type, non_generics)) {
            /* HELLO I AM NOT GENERIC DO NOT COPY ME. */
            type->var_temp_other = TYPEEXP_NON_GENERIC_MARKER;
            return type;
        } else {
            /* HELLO I AM GENERIC, FIRST TIME YOU MEET ME. */
            type->var_temp_other = MakeTypeVar(arena);
            return type->var_temp_other;
        }
    } else {
        /* HELLO I AM TYPE OPERATOR COPY ME FOR MY LEAVES. */
        struct TypeExp *fresh_type;
        fresh_type = ArenaAllocate(arena, sizeof(struct TypeExp));
        fresh_type->type = type->type;
        fresh_type->name = type->name;
        if (type->arg_first) {
            fresh_type->arg_first = MakeFreshTypeExpCopy(
                arena,
                type->arg_first,
                non_generics
            );
        }
        if (type->arg_second) {
            fresh_type->arg_second = MakeFreshTypeExpCopy(
                arena,
                type->arg_second,
                non_generics
            );
        }
        return fresh_type;
    }
}

void
MakeFreshTypeExpCleanup(struct TypeExp *type)
{
    type = PruneTypeExp(type);
    if (type->type == TYPEEXP_VARIABLE) {
        type->var_temp_other = NULL;
    } else {
        MakeFreshTypeExpCleanup(type->arg_first);
        MakeFreshTypeExpCleanup(type->arg_second);
    }
}

struct TypeExp *
MakeFreshTypeExp(struct Arena *arena, struct TypeExp *type,
                 struct NonGenericTypeList *non_generics)
{
    struct TypeExp *fresh_type;
    fresh_type = MakeFreshTypeExpCopy(arena, type, non_generics);
    MakeFreshTypeExpCleanup(type);
    return fresh_type;
}

static struct TypeExp IntegerTypeExp =
    { TYPEEXP_OPERATOR, -1, "int", {NULL}, {NULL}};
static struct TypeExp BooleanTypeExp =
    { TYPEEXP_OPERATOR, -2, "bool", {NULL}, {NULL}};


/*
 * Type Environments
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct TypeEnvironment {
    struct TypeEnvironment *parent;
    struct TypeExp *type;
    Symbol id;
};

#pragma GCC diagnostic pop

struct TypeEnvironment *BindType(struct Arena *arena,
                                 struct TypeEnvironment *parent,
                                 Symbol id,
                                 struct TypeExp *type);
struct TypeExp *LookupType(struct Arena *arena, struct TypeEnvironment *env,
                           Symbol id,
                           struct NonGenericTypeList *non_generics);

struct TypeEnvironment *
BindType(struct Arena *arena, struct TypeEnvironment *parent, Symbol id,
         struct TypeExp *type)
{
    struct TypeEnvironment *result;
    result = ArenaAllocate(arena, sizeof(struct TypeEnvironment));
    result->parent = parent;
    result->id = id;
    result->type = type;
    return result;
}

struct TypeExp *
LookupType(struct Arena *arena, struct TypeEnvironment *env, Symbol id,
           struct NonGenericTypeList *non_generics)
{
    while(env != NULL) {
        if (env->id == id) {
            return MakeFreshTypeExp(arena, env->type, non_generics);
        }
        env = env->parent;
    }
    return NULL;
}

/*
 * Type checking/inference
 */
struct CheckContext {
    struct Arena *arena;
    struct Errors *errors;
};

struct CheckContext *MakeNewCheckContext(struct Arena *arena);
void ReportTypeError(struct CheckContext *context, struct Expression *node,
                     char *error);
void Unify(struct CheckContext *context, struct Expression *node,
           struct TypeExp *type_one, struct TypeExp *type_two);
struct TypeExp *Analyze(struct CheckContext *context, struct Expression *node,
                        struct TypeEnvironment *env,
                        struct NonGenericTypeList *non_generics);

struct CheckContext *
MakeNewCheckContext(struct Arena *arena)
{
    struct CheckContext *result;
    result = ArenaAllocate(arena, sizeof(struct CheckContext));
    result->arena = arena;
    return result;
}

void
ReportTypeError(struct CheckContext *context, struct Expression *node, char *error)
{
    // TODO: Source locations.
    (void)node;
    AddErrorF(&context->errors, 0, 0, "Type error: %s", error);
}

void
Unify(struct CheckContext *context, struct Expression *node,
      struct TypeExp *type_one, struct TypeExp *type_two)
{
    type_one = PruneTypeExp(type_one);
    type_two = PruneTypeExp(type_two);
    if (type_one->type == TYPEEXP_VARIABLE) {
        if (type_one == type_two) { return; }
        if (IsTypeContainedWithin(type_one, type_two)) {
            ReportTypeError(context, node, "Type Error: Type Contained In Type?");
        } else {
            type_one->var_instance = type_two;
        }
    } else {
        if (type_two->type == TYPEEXP_VARIABLE) {
            Unify(context, node, type_two, type_one);
        } else {
            if (type_one->name != type_two->name) {
                ReportTypeError(context, node, "Mismatched types?");
            } else {
                if (type_one->arg_first) {
                    Unify(
                        context,
                        node,
                        type_one->arg_first,
                        type_two->arg_first
                    );
                }
                if (type_one->arg_second) {
                    Unify(
                        context,
                        node,
                        type_one->arg_second,
                        type_two->arg_second
                    );
                }
            }
        }
    }
}

struct TypeExp *
Analyze(struct CheckContext *context, struct Expression *node,
        struct TypeEnvironment *env, struct NonGenericTypeList *non_generics)
{
    switch(node->type) {
    case EXP_IDENTIFIER:
        {
            struct TypeExp *type = LookupType(
                context->arena,
                env,
                node->identifier_id,
                non_generics
            );
            if (type == NULL) {
                ReportTypeError(context, node, "Unbound identifier");
                return NULL;
            }
            return type;
        }
    case EXP_APPLY:
        {
            struct TypeExp *function_type = Analyze(
                context,
                node->apply_function,
                env,
                non_generics
            );
            struct TypeExp *arg_type = Analyze(
                context,
                node->apply_argument,
                env,
                non_generics
            );
            if (NULL == function_type || NULL == arg_type) {
                return NULL;
            }
            struct TypeExp *result_type = MakeTypeVar(context->arena);
            Unify(
                context,
                node,
                MakeFunctionTypeExp(
                    context->arena,
                    arg_type,
                    result_type
                ),
                function_type);
            return result_type;
        }
    case EXP_LAMBDA:
        {
            struct TypeExp *arg_type = MakeTypeVar(context->arena);
            struct TypeEnvironment *new_env = BindType(
                context->arena,
                env,
                node->lambda_id,
                arg_type
            );
            struct NonGenericTypeList *new_non_generic = ExtendNonGenericTypeList(
                context->arena,
                arg_type,
                non_generics
            );
            struct TypeExp *result_type = Analyze(
                context,
                node->lambda_body,
                new_env,
                new_non_generic
            );
            return MakeFunctionTypeExp(context->arena, arg_type, result_type);
        }
    case EXP_LET:
        {
            struct TypeExp *defn_type = Analyze(
                context,
                node->let_value,
                env,
                non_generics
            );
            struct TypeEnvironment *new_env = BindType(
                context->arena,
                env,
                node->let_id,
                defn_type
            );
            return Analyze(context, node->let_body, new_env, non_generics);
        }
    case EXP_LETREC:
        {
            struct TypeExp *new_type = MakeTypeVar(context->arena);
            struct TypeEnvironment *new_env = BindType(
                context->arena,
                env,
                node->let_id,
                new_type
            );
            struct NonGenericTypeList *new_non_generic = ExtendNonGenericTypeList(
                context->arena,
                new_type,
                non_generics
            );
            struct TypeExp *defn_type = Analyze(
                context,
                node->let_value,
                new_env,
                new_non_generic
            );
            Unify(context, node, new_type, defn_type);
            return Analyze(context, node->let_body, new_env, non_generics);
        }
    case EXP_INTEGER_CONSTANT:
        return &IntegerTypeExp;
    case EXP_TRUE:
    case EXP_FALSE:
        return &BooleanTypeExp;
    case EXP_IF:
    case EXP_THEN_ELSE:
    case EXP_BINARY:
    case EXP_UNARY:
    case EXP_INVALID:
        break;
    }
    Fail("Invalid expression structure");
    return NULL;
}

/*
 * Driver
 */

void PrintTypeExpression(struct TypeExp *type);
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

int main()
{
    struct MString *buffer = MStringCreate(
        "let rec factorial =\n"
        "  fn n => if n = 0 then 1 else n * factorial n - 1\n"
        "in factorial 5\n"
    );

    struct Errors *errors;
    struct MillieTokens *tokens = LexBuffer(buffer, &errors);
    if (errors) {
        PrintErrors("test", tokens, errors);
        return 1;
    }

    PrintTokens(tokens);

    /* struct Arena *arena = MakeFreshArena(); */
    /* struct CheckContext *context = MakeNewCheckContext(arena); */
    /* struct Expression *node = ParseExpression(arena, tokens); */
    /* struct TypeExp *type = Analyze(context, node, NULL, NULL); */
    /* PrintTypeExpression(type); */
    /* FreeArena(&arena); */

    return 0;
}
