#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct CompileBinding {
    Symbol symbol;
    uint8_t reg;
};

struct CompileContext {
    uint8_t *code;
    uint8_t *code_write;
    int code_capacity;

    uint8_t integer_registers;
    size_t max_registers;

    struct CompileBinding *bindings;
    int binding_top;
    int binding_capacity;

    struct Errors **errors;
    struct MillieTokens *tokens;
};

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

static void _WriteInstructionU8(struct CompileContext *context, uint8_t value)
{
    _EnsureCodeCapacity(context, 1);
    *(context->code_write++) = value;
}

static void _WriteInstructionU16(struct CompileContext *context, uint16_t value)
{
    _EnsureCodeCapacity(context, 4);
    *(context->code_write++) = (value & 0x0000FF) >> 0;
    *(context->code_write++) = (value & 0x00FF00) >> 8;
}

static void _WriteInstructionU32(struct CompileContext *context, uint32_t value)
{
    _EnsureCodeCapacity(context, 4);
    *(context->code_write++) = (value & 0x000000FF) >> 0;
    *(context->code_write++) = (value & 0x0000FF00) >> 8;
    *(context->code_write++) = (value & 0x00FF0000) >> 16;
    *(context->code_write++) = (value & 0xFF000000) >> 24;
}

static void _WriteInstructionU64(struct CompileContext *context, uint64_t value)
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

static uint8_t _CompileExpression(struct CompileContext *context,
                                   struct Expression *expression);

static uint8_t _CompileIntegerLiteral(struct CompileContext *context,
                                      struct Expression *expression)
{
    uint8_t reg = _GetFreeIntRegister(context);
    if (expression->literal_value <= UINT8_MAX) {
        _WriteInstructionU8(context, OP_LOADI_8);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU8(context, expression->literal_value);
    } else if (expression->literal_value <= UINT16_MAX) {
        _WriteInstructionU8(context, OP_LOADI_16);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU16(context, expression->literal_value);
    } else if (expression->literal_value <= UINT32_MAX) {
        _WriteInstructionU8(context, OP_LOADI_32);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU32(context, expression->literal_value);
    } else {
        _WriteInstructionU8(context, OP_LOADI_64);
        _WriteInstructionU8(context, reg);
        _WriteInstructionU64(context, expression->literal_value);
    }
    return reg;
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


static uint8_t _CompileExpression(struct CompileContext *context,
                                  struct Expression *expression)
{
    switch(expression->type) {
    case EXP_INTEGER_CONSTANT: return _CompileIntegerLiteral(context, expression);
    case EXP_LET: return _CompileLet(context, expression);
    case EXP_IDENTIFIER: return _CompileIdentifier(context, expression);
    case EXP_ERROR: break;
    case EXP_LAMBDA:
    case EXP_APPLY:
    case EXP_LETREC:
    case EXP_TRUE:
    case EXP_FALSE:
    case EXP_IF:
    case EXP_BINARY:
    case EXP_UNARY:
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

#define INITIAL_BINDING_CAPACITY (64)
#define INITIAL_CODE_CAPACITY (64)

void CompileExpression(struct Expression *expression,
                       struct MillieTokens *tokens,
                       struct Errors **errors,
                       struct CompiledExpression *result)
{
    struct CompileContext context;
    context.code = malloc(INITIAL_CODE_CAPACITY);
    context.code_write = context.code;
    context.code_capacity = INITIAL_CODE_CAPACITY;

    context.integer_registers = 0;
    context.max_registers = 0;

    context.bindings = malloc(INITIAL_BINDING_CAPACITY);
    context.binding_top = 0;
    context.binding_capacity = INITIAL_BINDING_CAPACITY;

    context.errors = errors;
    context.tokens = tokens;

    result->result_register = _CompileExpression(&context, expression);
    _WriteInstructionU8(&context, OP_HALT);

    result->code = context.code;
    result->code_length = context.code_write - context.code;
    result->register_count = context.max_registers;
}
