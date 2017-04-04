#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

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

    Symbol *closure_symbols;
    int closure_top;
    int closure_capacity;

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

    context->closure_symbols = malloc(INITIAL_BINDING_CAPACITY);
    context->closure_top = 0;
    context->closure_capacity = INITIAL_BINDING_CAPACITY;

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
        context->code_write = context->code + code_len;
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

static void _RetainRegister(struct CompileContext *context, uint8_t reg)
{
    (void)(context);
    (void)(reg);
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
    _RetainRegister(context, reg);
    context->binding_top++;
}

static void _PopBinding(struct CompileContext *context)
{
    context->binding_top--;
    _FreeRegister(context, context->bindings[context->binding_top].reg);
}

static void _FinishCompile(struct CompileContext *context,
                           uint8_t result_register,
                           struct CompiledExpression *result)
{
    result->result_register = result_register;
    _WriteCodeU8(context, OP_RET);

    result->code = context->code;
    result->code_length = context->code_write - context->code;
    context->code = NULL;

    result->closure = context->closure_symbols;
    result->closure_length = context->closure_top;

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
        _WriteCodeU8(context, value);
    } else if (value <= UINT16_MAX) {
        _WriteCodeU8(context, OP_LOADI_16);
        _WriteCodeU16(context, value);
    } else if (value <= UINT32_MAX) {
        _WriteCodeU8(context, OP_LOADI_32);
        _WriteCodeU32(context, value);
    } else {
        _WriteCodeU8(context, OP_LOADI_64);
        _WriteCodeU64(context, value);
    }
    _WriteCodeU8(context, reg);
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

static uint8_t _CompileLambdaImpl(struct CompileContext *context,
                                  struct Expression *expression,
                                  uint8_t closure_register);

static uint8_t _CompileLetRec(struct CompileContext *context,
                              struct Expression *expression)
{
    // `let rec` requires special handling. In the fullness of time, we should
    // loosen the restrictions on `let rec`, but for now we stick with this,
    // which is a fine time-honored tradition dating back to Standard ML.
    uint8_t dest_reg = _GetFreeIntRegister(context);
    if (expression->let_value->type != EXP_LAMBDA) {
        _ReportCompileError(
            context,
            expression->let_value,
            "the expression in a let rec must be a function definition"
        );
        return dest_reg;
    }

    // Bind the identifier to our target register, and then compile the lambda,
    // knowing that the closure object goes straight into the target
    // register. That way, if the closure refers to itself, it will have the
    // right pointer already in the right place.
    _PushBinding(context, expression->let_id, dest_reg);
    _CompileLambdaImpl(context, expression->let_value, dest_reg);

    // Now we can compile the body.
    uint8_t body_reg = _CompileExpression(context, expression->let_body);
    _PopBinding(context);
    return body_reg;
}

static uint8_t _CompileIdentifierImpl(struct CompileContext *context, Symbol id)
{
    // Look to see if it's a local that's in a register.
    for(int i = context->binding_top; i >= 0; i--) {
        if (context->bindings[i].symbol == id) {
            _RetainRegister(context, context->bindings[i].reg);
            return context->bindings[i].reg;
        }
    }

    // Must be in our closure.
    //
    // TODO: It would be great if we could save the result of this load and not
    // do it all the time but I'm not entirely sure how...
    //
    // Closure is always in r0.
    int closure_offset = -1;
    for (int i = 0; i < context->closure_top; i++) {
        if (context->closure_symbols[i] == id) {
            closure_offset = i;
            break;
        }
    }

    if (closure_offset < 0) {
        if (context->closure_capacity == context->closure_top) {
            uint32_t new_capacity = context->closure_capacity * 2;
            context->closure_symbols = realloc(
                context->closure_symbols,
                new_capacity * sizeof(Symbol)
            );
            context->closure_capacity = new_capacity;
        }
        closure_offset = context->closure_top;
        context->closure_symbols[closure_offset] = id;
        context->closure_top++;
    }

    uint8_t load_target = _GetFreeIntRegister(context);

    // Load from the closure in r0. Note that closure_offset is +1 because
    // we're skipping over the function pointer which is at r0+0.
    _WriteCodeU8(context, OP_LOADA_64);
    _WriteCodeU8(context, 0);
    _WriteCodeU16(context, (int16_t)closure_offset + 1);
    _WriteCodeU8(context, load_target);

    return load_target;
}


static uint8_t _CompileIdentifier(struct CompileContext *context,
                                     struct Expression *expression)
{
    return _CompileIdentifierImpl(context, expression->identifier_id);
}

static uint8_t _CompileLambdaImpl(struct CompileContext *context,
                                  struct Expression *expression,
                                  uint8_t closure_register)
{
    // First, compile the actual function.
    // TODO: Optimize the case where no closure is required:
    //   - Generate no allocation
    //   - Don't use r0?
    int func_id;
    struct CompiledExpression *result = _AddFunction(context->module, &func_id);
    {
        struct CompileContext child_context;
        _InitCompileContext(&child_context, context);

        // Reserve register 0 for the closure.
        child_context.integer_registers++;

        // And the next one for the arg...
        uint8_t arg_register = _GetFreeIntRegister(&child_context);
        _PushBinding(&child_context, expression->lambda_id, arg_register);
        uint8_t ret_register = _CompileExpression(
            &child_context,
            expression->lambda_body
        );
        _PopBinding(&child_context);
        _FinishCompile(&child_context, ret_register, result);
    }

    // Now generate the closure object into `closure_register`.
    _WriteCodeU8(context, OP_ALLOCA_64);
    _WriteCodeU64(context, result->closure_length + 1);
    _WriteCodeU8(context, closure_register);

    // And write out our stuffs. First thing in a closure is
    // the pointer to code.
    uint8_t id_reg = _WriteLoadLiteral(context, func_id);

    _WriteCodeU8(context, OP_STOREA_64);
    _WriteCodeU8(context, closure_register);
    _WriteCodeU16(context, 0);
    _WriteCodeU8(context, id_reg);

    _FreeRegister(context, id_reg);

    for(size_t i = 0; i < result->closure_length; i++) {
        id_reg = _CompileIdentifierImpl(context, result->closure[i]);

        _WriteCodeU8(context, OP_STOREA_64);
        _WriteCodeU8(context, closure_register);
        _WriteCodeU16(context, i + 1);
        _WriteCodeU8(context, id_reg);

        _FreeRegister(context, id_reg);
    }

    return closure_register;
}


static uint8_t _CompileLambda(struct CompileContext *context,
                              struct Expression *expression)
{
    uint8_t closure_register = _GetFreeIntRegister(context);
    return _CompileLambdaImpl(context, expression, closure_register);
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

static uint8_t _CompileIf(struct CompileContext *context,
                          struct Expression *expression)
{
    // This is a little bit complicated, so follow carefully.
    //
    // First, compile the test part of the if.  This will leave a boolean in
    // result_reg.
    uint8_t result_reg = _CompileExpression(context, expression->if_test);

    // Now we write in a JZ(<result_reg>, <false_branch>) which will jump to
    // <false_branch> if the test is false...
    _WriteCodeU8(context, OP_JZ);
    _WriteCodeU8(context, result_reg);

    // ...but we need to skip over the <false_branch> offset, which is a 16b
    // signed code offset from the location *after* the address. (So 0 does
    // nothing.)
    const ptrdiff_t false_target_loc = context->code_write - context->code;
    _EnsureCodeCapacity(context, 2);
    context->code_write += 2;

    // We don't need the comparison register anymore.
    _FreeRegister(context, result_reg);

    // Now compile the true branch, and remember where we left the value.
    uint8_t true_reg = _CompileExpression(context, expression->if_then);

    // Now we need to stick in a jump over the false branch, but we don't
    // know where it's going yet, so do the same trick where we leave a hole...
    _WriteCodeU8(context, OP_JMP);
    const ptrdiff_t end_target_loc = context->code_write - context->code;
    _EnsureCodeCapacity(context, 2);
    context->code_write += 2;

    // Ah, here's where the false branch is. Compute the offset from the start
    // of the code block...
    const ptrdiff_t false_loc = context->code_write - context->code;

    // ...and now the offset from the OP_JZ location, above, and write it in.
    int16_t false_offset = false_loc - false_target_loc - 2;
    context->code_write = context->code + false_target_loc;
    _WriteCodeU16(context, false_offset);
    context->code_write = context->code + false_loc;

    // Now compile the false branch.
    uint8_t false_reg = _CompileExpression(context, expression->if_else);

    // If false put the data somewhere different than true, then we need to make
    // it so the output is in the same register. Issue a MOV and then we can
    // free the false register.
    if (false_reg != true_reg) {
        _WriteCodeU8(context, OP_MOV);
        _WriteCodeU8(context, false_reg);
        _WriteCodeU8(context, true_reg);
        _FreeRegister(context, false_reg);
    }

    // Now we're at the end of the block, so compute the offset from the start
    // of the code block...
    const ptrdiff_t end_loc = context->code_write - context->code;

    // ...and now the offset from the OP_JMP location at the end of the <true>
    // branch. Same semantics, same basic idea.
    int16_t end_offset = end_loc - end_target_loc - 2;
    context->code_write = context->code + end_target_loc;
    _WriteCodeU16(context, end_offset);
    context->code_write = context->code + end_loc;

    // ...and the result got left in the <true> register.
    return true_reg;
}

static uint8_t _CompileTrue(struct CompileContext *context)
{
    return _WriteLoadLiteral(context, 1);
}

static uint8_t _CompileFalse(struct CompileContext *context)
{
    return _WriteLoadLiteral(context, 0);
}

static uint8_t _CompileExpression(struct CompileContext *context,
                                  struct Expression *expression)
{
    switch(expression->type) {
    case EXP_INTEGER_CONSTANT: return _CompileIntegerLiteral(context, expression);
    case EXP_LET: return _CompileLet(context, expression);
    case EXP_LETREC: return _CompileLetRec(context, expression);
    case EXP_IDENTIFIER: return _CompileIdentifier(context, expression);
    case EXP_LAMBDA: return _CompileLambda(context, expression);
    case EXP_APPLY: return _CompileApply(context, expression);
    case EXP_BINARY: return _CompileBinary(context, expression);
    case EXP_UNARY: return _CompileUnary(context, expression);
    case EXP_IF: return _CompileIf(context, expression);
    case EXP_TRUE: return _CompileTrue(context);
    case EXP_FALSE: return _CompileFalse(context);

    case EXP_ERROR:
        break;

    case EXP_INVALID:
        _ReportCompileError(
            context,
            expression,
            "Unsupported expression during compilation"
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
