// TODO: Memory allocator
void *Allocate(size_t size);

// AST
typedef enum {
    EXP_INVALID = 0,
    EXP_LAMBDA = 1,
    EXP_IDENTIFIER = 2,
    EXP_APPLY = 3,
    EXP_LET = 4,
    EXP_LETREC = 5,
} ExpressionType;

typedef void *Identifier;

struct Expression {
    ExpressionType type;
    Identifier id;
    struct Expression *first;
    struct Expression *second;
};

struct Expression *
NewExpression()
{
    return (struct Expression *)Allocate(sizeof(struct Expression));
}

struct Expression *
MakeLambda(Identifier variable, struct Expression *body)
{
    struct Expression *result = NewExpression();
    result->type = EXP_LAMBDA;
    result->id = variable;
    result->first = body;
    return result;
}

struct Expression *
MakeIdentifier(Identifier id)
{
    struct Expression *result = NewExpression();
    result->type = EXP_IDENTIFIER;
    result->id = variable;
    return result;
}

struct Expression *
MakeApply(struct Expression *func_expr, struct Expression *arg_expr)
{
    struct Expression *result = NewExpression();
    result->type = EXP_APPLY;
    result->first = func_expr;
    result->second = arg_expr;
    return result;
}

struct Expression *
MakeLet(Identifier variable, struct Expression *value, struct Expression *body)
{
    struct Expression *result = NewExpression();
    result->type = EXP_LET;
    result->id = variable;
    result->first = value;
    result->second = body;
    return result;
}

struct Expression *
MakeLetRec(Identifier variable, struct Expression *value,
           struct Expression *body)
{
    struct Expression *result = NewExpression();
    result->type = EXP_LETREC;
    result->id = variable;
    result->first = value;
    result->second = body;
    return result;
}


// Type Variables
typedef enum {
    TYPEVAR_INVALID = 0,
    TYPEVAR_VARIABLE = 1,
    TYPEVAR_OPERATOR = 2,
} TypeVarType;

struct TypeVar {
    TypeVarType type;
    int id;
    char *name;
    struct TypeVar *args[2];
};

struct TypeVar *
MakeFunctionTypeVar(struct TypeVar *from_type, struct TypeVar *to_type) {
    struct TypeVar *result = (struct TypeVar *)Allocate(sizeof(struct TypeVar));
    result->type = TYPEVAR_OPERATOR;
    result->name = "->";
    result->args[0] = from_type;
    result->args[1] = to_type;
    return result;
}

struct TypeVar Integer = { TYPEVAR_OPERATOR, -1, "int", };
struct TypeVar Boolean = { TYPEVAR_OPERATOR, -2, "bool", };


// Environment
// This one will want a fast thingy?
