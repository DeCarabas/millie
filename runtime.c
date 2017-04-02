#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct Frame {
    uint64_t *registers;
};

//#define VM_TRACE

#ifdef VM_TRACE

static const char *_op_names[] = {
#define Q(x) #x
#define OPCODE(name)  Q(name) ,
#include "opcodes.inc"
#undef OPCODE
#undef Q
};

static void _TraceOp(const uint8_t *code,
                     struct CompiledExpression *def,
                     struct Frame *frame)
{
    printf("%p %s\n", code, _op_names[*code]);
    for(size_t i = 0; i < def->register_count; i++) {
        printf("  Frame: %p\n", frame);
        printf("    r%zu: 0x%llx\n", i, frame->registers[i]);
    }
}

#define TRACE_VM(ip, def, frame) _TraceOp(ip, def, frame)

#else

#define TRACE_VM(ip, def, frame)

#endif



static uint8_t _ReadU8(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 1;
    return buffer[0];
}

static uint16_t _ReadU16(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 2;
    return
        (((uint16_t)buffer[0]) << 0) |
        (((uint16_t)buffer[1]) << 8);
}

static uint32_t _ReadU32(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 4;
    return
        (((uint32_t)buffer[0]) << 0)  |
        (((uint32_t)buffer[1]) << 8)  |
        (((uint32_t)buffer[2]) << 16) |
        (((uint32_t)buffer[3]) << 24);
}

static uint64_t _ReadU64(const uint8_t **buffer_ptr) {
    const uint8_t *buffer = *buffer_ptr;
    *buffer_ptr += 8;
    return
        (((uint64_t)buffer[0]) << 0)  |
        (((uint64_t)buffer[1]) << 8)  |
        (((uint64_t)buffer[2]) << 16) |
        (((uint64_t)buffer[3]) << 24) |
        (((uint64_t)buffer[4]) << 32) |
        (((uint64_t)buffer[5]) << 40) |
        (((uint64_t)buffer[6]) << 48) |
        (((uint64_t)buffer[7]) << 56);
}

uint64_t EvaluateCode(struct CompiledExpression *code) {
    struct Frame frame;
    frame.registers = calloc(code->register_count, sizeof(uint64_t));

    const uint8_t *ip = code->code;
    bool halt = false;
    while(!halt) {
        TRACE_VM(ip, code, &frame);
        MILLIE_OPCODE op = *(ip++);
        switch(op) {
        case OP_LOADI_8:
            {
                uint8_t reg = _ReadU8(&ip);
                uint8_t val = _ReadU8(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_LOADI_16:
            {
                uint8_t reg = _ReadU8(&ip);
                uint16_t val = _ReadU16(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_LOADI_32:
            {
                uint8_t reg = _ReadU8(&ip);
                uint32_t val = _ReadU32(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_LOADI_64:
            {
                uint8_t reg = _ReadU8(&ip);
                uint64_t val = _ReadU64(&ip);
                frame.registers[reg] = val;
            }
            break;
        case OP_HALT:
            halt = true;
            break;

        default:
            fprintf(stderr, "ERROR: UNKNOWN INSTRUCTION: %d\n", op);
            halt = true;
            break;
        }
    }

    uint64_t result = frame.registers[code->result_register];
    free(frame.registers);
    return result;
}
