#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct CompileBinding {
    Symbol symbol;
    uint8_t reg;
};

void ModuleInit(struct Module *module) {
    memset(module, 0, sizeof(*module));
}

static struct CompiledExpression *_AddFunction(
    struct Module *global,
    int *expression_id
)
{
    if (global->function_count == global->function_capacity) {
        int new_cap;
        if (global->function_capacity == 0) {
            new_cap = 10;
        } else {
            new_cap = global->function_capacity * 2;
        }
        global->functions = realloc(
            global->functions,
            sizeof(struct CompiledExpression) * new_cap
        );
        global->function_capacity = new_cap;
    }

    *expression_id = global->function_count;
    global->function_count += 1;
    return global->functions + global->function_count - 1;
}

struct CompileContext {
    uint8_t *code;
    uint8_t *code_write;
    int code_capacity;

    uint8_t integer_registers;
    size_t max_registers;

    struct CompileBinding *bindings;
    int binding_top;
    int binding_capacity;

    struct CompileContext *parent_context;

    struct Module *module;
    struct MillieTokens *tokens;
    struct Errors **errors;
};

#define INITIAL_BINDING_CAPACITY (64)
#define INITIAL_CODE_CAPACITY (64)

static void _InitCompileContext(struct CompileContext *context,
                                struct CompileContext *parent)
{
    memset(context, 0, sizeof(*context));
    context->code = malloc(INITIAL_CODE_CAPACITY);
    context->code_write = context->code;
    context->code_capacity = INITIAL_CODE_CAPACITY;

    context->integer_registers = 0;
    context->max_registers = 0;

    context->bindings = malloc(INITIAL_BINDING_CAPACITY);
    context->binding_top = 0;
    context->binding_capacity = INITIAL_BINDING_CAPACITY;

    context->parent_context = parent;

    if (parent != NULL) {
        context->module = parent->module;
        context->tokens = parent->tokens;
        context->errors = parent->errors;
    }
}

static void _ReportCompileError(struct CompileContext *context,
                                struct Expression *expression,
                                const char *error)
{
    struct MillieToken start_token = GetToken(
        context->tokens,
        expression->start_token
    );
    struct MillieToken end_token = GetToken(
        context->tokens,
        expression->end_token
    );

    struct MStringStatic st;
    struct MString *error_string = MStringCreateStatic(error, &st);
    AddError(
        context->errors,
        start_token.start,
        end_token.start + end_token.length,
        error_string);
}

static void _EnsureCodeCapacity(struct CompileContext *context, int more)
{
    int code_len = context->code_write - context->code;
    if (context->code_capacity - code_len < more) {
        uint32_t new_capacity = context->code_capacity * 2;
        context->code = realloc(context->code, new_capacity);
        context->code_capacity = new_capacity;
    }
}

static void _WriteCodeU8(struct CompileContext *context, uint8_t value)
{
    _EnsureCodeCapacity(context, 1);
    *(context->code_write++) = value;
}

static void _WriteCodeU16(struct CompileContext *context, uint16_t value)
{
    _EnsureCodeCapacity(context, 4);
    *(context->code_write++) = (value & 0x0000FF) >> 0;
    *(context->code_write++) = (value & 0x00FF00) >> 8;
}

static void _WriteCodeU32(struct CompileContext *context, uint32_t value)
{
    _EnsureCodeCapacity(context, 4);
    *(context->code_write++) = (value & 0x000000FF) >> 0;
    *(context->code_write++) = (value & 0x0000FF00) >> 8;
    *(context->code_write++) = (value & 0x00FF0000) >> 16;
    *(context->code_write++) = (value & 0xFF000000) >> 24;
}

static void _WriteCodeU64(struct CompileContext *context, uint64_t value)
{
    _EnsureCodeCapacity(context, 8);
    *(context->code_write++) = (value & 0x00000000000000FF) >> 0;
    *(context->code_write++) = (value & 0x000000000000FF00) >> 8;
    *(context->code_write++) = (value & 0x0000000000FF0000) >> 16;
    *(context->code_write++) = (value & 0x00000000FF000000) >> 24;
    *(context->code_write++) = (value & 0x000000FF00000000) >> 32;
    *(context->code_write++) = (value & 0x0000FF0000000000) >> 40;
    *(context->code_write++) = (value & 0x00FF000000000000) >> 48;
    *(context->code_write++) = (value & 0xFF00000000000000) >> 56;
}

static uint8_t _GetFreeIntRegister(struct CompileContext *context)
{
    context->integer_registers++;
    context->max_registers++;
    return context->integer_registers - 1;
}

static void _FreeRegister(struct CompileContext *context, uint8_t reg)
{
    (void)(context);
    (void)(reg);
}

static void _PushBinding(struct CompileContext *context,
                         Symbol symbol,
                         uint8_t reg)
{
    if (context->binding_top == context->binding_capacity) {
        context->binding_capacity *= 2;
        context->bindings = realloc(context->code, context->binding_capacity);
    }

    context->bindings[context->binding_top].symbol = symbol;
    context->bindings[context->binding_top].reg = reg;
    context->binding_top++;
}

static void _PopBinding(struct CompileContext *context)
{
    context->binding_top--;
}

static void _FinishCompile(struct CompileContext *context,
                           uint8_t result_register,
                           struct CompiledExpression *result)
{
    result->result_register = result_register;
    _WriteCodeU8(context, OP_HALT);

    result->code = context->code;
    result->code_length = context->code_write - context->code;

    context->code = NULL;
    result->register_count = context->max_registers;

    free(context->bindings);
    memset(context, 0, sizeof(struct CompileContext));
}


static uint8_t _CompileExpression(struct CompileContext *context,
                                   struct Expression *expression);

static uint8_t _WriteLoadLiteral(struct CompileContext *context, uint64_t value)
{
    uint8_t reg = _GetFreeIntRegister(context);
    if (value <= UINT8_MAX) {
        _WriteCodeU8(context, OP_LOADI_8);
        _WriteCodeU8(context, reg);
        _WriteCodeU8(context, value);
    } else if (value <= UINT16_MAX) {
        _WriteCodeU8(context, OP_LOADI_16);
        _WriteCodeU8(context, reg);
        _WriteCodeU16(context, value);
    } else if (value <= UINT32_MAX) {
        _WriteCodeU8(context, OP_LOADI_32);
        _WriteCodeU8(context, reg);
        _WriteCodeU32(context, value);
    } else {
        _WriteCodeU8(context, OP_LOADI_64);
        _WriteCodeU8(context, reg);
        _WriteCodeU64(context, value);
    }
    return reg;
}

static uint8_t _CompileIntegerLiteral(struct CompileContext *context,
                                      struct Expression *expression)
{
    return _WriteLoadLiteral(context, expression->literal_value);
}

static uint8_t _CompileLet(struct CompileContext *context,
                           struct Expression *expression)
{
    uint8_t dest_reg = _CompileExpression(context, expression->let_value);
    _PushBinding(context, expression->let_id, dest_reg);
    uint8_t result = _CompileExpression(context, expression->let_body);
    _PopBinding(context);
    return result;
}

static uint8_t _CompileIdentifier(struct CompileContext *context,
                                     struct Expression *expression)
{
    for(int i = context->binding_top; i >= 0; i--) {
        if (context->bindings[i].symbol == expression->identifier_id) {
            return context->bindings[i].reg;
        }
    }

    _ReportCompileError(context, expression, "Unbound identifier");
    return 0;
}

static uint8_t _CompileLambda(struct CompileContext *context,
                              struct Expression *expression)
{
    // Compile the lambda.
    int func_id;
    struct CompiledExpression *result = _AddFunction(context->module, &func_id);
    {
        struct CompileContext child_context;
        _InitCompileContext(&child_context, context);

        uint8_t arg_register = _GetFreeIntRegister(&child_context);
        _PushBinding(&child_context, expression->lambda_id, arg_register);
        uint8_t ret_register = _CompileExpression(
            &child_context,
            expression->lambda_body
        );
        _PopBinding(&child_context);
        _FinishCompile(&child_context, ret_register, result);
    }

    return _WriteLoadLiteral(context, func_id);
}

static uint8_t _CompileApply(struct CompileContext *context,
                             struct Expression *expression)
{
    // TODO: OPT: Direct call opcode where func id is embedded in instruction
    //            stream rather than register.
    // TODO: Register allocation for reals; track references on registers and
    //       free them when they aren't needed anymore.
    uint8_t lambda_register = _CompileExpression(
        context,
        expression->apply_function
    );
    uint8_t arg_register = _CompileExpression(
        context,
        expression->apply_argument
    );
    uint8_t ret_register = _GetFreeIntRegister(context);

    _WriteCodeU8(context, OP_CALL);
    _WriteCodeU8(context, lambda_register);
    _WriteCodeU8(context, arg_register);
    _WriteCodeU8(context, ret_register);

    return ret_register;
}

static uint8_t _CompileBinary(struct CompileContext *context,
                              struct Expression *expression)
{
    // opcodes we use are guaranteed to read their input before writing their
    // output, so we can free the input registers before allocating the output
    // register to save space.
    uint8_t left_register = _CompileExpression(
        context,
        expression->binary_left
    );
    uint8_t right_register = _CompileExpression(
        context,
        expression->binary_right
    );
    _FreeRegister(context, left_register);
    _FreeRegister(context, right_register);

    uint8_t out_register = _GetFreeIntRegister(context);

    switch(expression->binary_operator) {
    case TOK_PLUS: _WriteCodeU8(context, OP_ADD); break;
    case TOK_MINUS: _WriteCodeU8(context, OP_SUB); break;
    case TOK_EQUALS: _WriteCodeU8(context, OP_EQ); break;
    case TOK_STAR: _WriteCodeU8(context, OP_MUL); break;
    default:
        _ReportCompileError(context, expression, "Unsupported binary operator");
        break;
    }

    _WriteCodeU8(context, left_register);
    _WriteCodeU8(context, right_register);
    _WriteCodeU8(context, out_register);

    return out_register;
}

static uint8_t _CompileUnary(struct CompileContext *context,
                             struct Expression *expression)
{
    // opcodes we use are guaranteed to read their input before writing their
    // output, so we can free the input registers before allocating the output
    // register to save space.
    uint8_t arg_register = _CompileExpression(
        context,
        expression->unary_arg
    );
    _FreeRegister(context, arg_register);

    uint8_t out_register = _GetFreeIntRegister(context);

    switch(expression->binary_operator) {
    case TOK_MINUS: _WriteCodeU8(context, OP_NEG); break;
    default:
        _ReportCompileError(context, expression, "Unsupported unary operator");
        break;
    }

    _WriteCodeU8(context, arg_register);
    _WriteCodeU8(context, out_register);

    return out_register;
}

static uint8_t _CompileExpression(struct CompileContext *context,
                                  struct Expression *expression)
{
    switch(expression->type) {
    case EXP_INTEGER_CONSTANT: return _CompileIntegerLiteral(context, expression);
    case EXP_LET: return _CompileLet(context, expression);
    case EXP_IDENTIFIER: return _CompileIdentifier(context, expression);
    case EXP_LAMBDA: return _CompileLambda(context, expression);
    case EXP_APPLY: return _CompileApply(context, expression);
    case EXP_BINARY: return _CompileBinary(context, expression);
    case EXP_UNARY: return _CompileUnary(context, expression);

    case EXP_ERROR:
        break;

    case EXP_LETREC:
    case EXP_TRUE:
    case EXP_FALSE:
    case EXP_IF:
    case EXP_INVALID:
        _ReportCompileError(
            context,
            expression,
            "Unsupported compile expression"
        );
        break;
    }
    return 0;
}

int CompileExpression(struct Expression *expression,
                      struct MillieTokens *tokens,
                      struct Errors **errors,
                      struct Module *module)
{


    struct CompileContext context;

    int func_id;
    struct CompiledExpression *result = _AddFunction(module, &func_id);

    _InitCompileContext(&context, NULL);
    context.module = module;
    context.errors = errors;
    context.tokens = tokens;

    uint8_t result_register = _CompileExpression(&context, expression);
    _FinishCompile(&context, result_register, result);
    return func_id;
}
