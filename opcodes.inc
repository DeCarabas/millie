// Opcodes for the millie bytecode VM.
// These are expected to be included in somewhere that defines OPCODE.
// The OPCODE macro is defined as:
//
//    OPCODE(symbol, arg0, arg1, arg2)
//
// The <arg> bits are currently just to inform the disassembler; nothing else
// uses them, although they should obviously reflect what the compiler writes
// and what the VM reads. The various arg types are:
//
//   U8, U16, U32, U64: Unsigned constant values.
//   REG:               A register to read from.
//   DREG:              A destination register.
//   OFF:               A 16-bit signed offset.
//
// Note that all destination registers *may* re-use source registers; the
// machine is guaranteed to read all source registers before writing to the
// destination registers.
//
// == The instructions ==
//
// RET returns from the current frame.
OPCODE(RET, 0, 0, 0)

// LOADI loads an immediate out of the instruction stream into a register.
OPCODE(LOADI_8,  U8,  DREG, 0)
OPCODE(LOADI_16, U16, DREG, 0)
OPCODE(LOADI_32, U32, DREG, 0)
OPCODE(LOADI_64, U64, DREG, 0)

// LOADA dereferences an offset address from a register, as if it were an
// array of that type. For example, LOADA_64(0, 7, 6) is like the C expression
// r6 = ((uint64_t *)r0)[7].
OPCODE(LOADA_64, REG, IDX, DREG)

// STOREA is the opposite of LOADA-- it write to memory instead. For example,
// STOREA_64(0, 7, 6) is like the C expression ((uint64_t *)r0)[7] = r6
OPCODE(STOREA_64, REG, IDX, REG)

// NEW_CLOSURE allocates a new closure for the function id in the specified
// register, and stores the allocated closure in the specified register.
//
// In memory, a closure is a pointer to an array of 64-bit slots, the first of
// which contains the function ID. The rest of the slots contain the closed
// values.
//
// (The runtime and the compiler both know how big the closure object needs to
// be based on the function ID.)
OPCODE(NEW_CLOSURE, REG, DREG, 0)

// NEW_TUPLE allocates a new tuple with the length in the first register, and
// stores the allocated tuple in the second register.
//
// In memory, a tuple is a struct with a field for each slot, laid end to end.
//
OPCODE(NEW_TUPLE, REG, DREG, 0)

// CALL calls a function, indicated by the closure pointer in the first
// register. The single argument value is in the second register, and the
// third register has the destination for the resulting value.
OPCODE(CALL, REG, REG, DREG)

// ADD, SUB, and MUL are simple arithmetic from register to register.
OPCODE(ADD, REG, REG, DREG)
OPCODE(SUB, REG, REG, DREG)
OPCODE(MUL, REG, REG, DREG)

// NEG is unary negation.
OPCODE(NEG, REG, DREG, 0)

// EQ compares the two registers for equality and writes a 1 or a 0 in the
// destination register.
OPCODE(EQ, REG, REG, DREG)

// JMP jumps to another location. The location is specified as an offset from
// the end of the entire instruction. For example, if the 'JMP' byte is at
// offset 7 and the OFF argument is 5, then the destination will be offset 15:
// 7 (the starting point) + 5 (the offset) + 1 (for the opcode) + 2 (for the
// offset).
OPCODE(JMP, OFF,  0,   0)

// JZ jumps to another location if the specified register contains zero. The
// jump target is computed as it is for JMP, above.
OPCODE(JZ,  REG,  OFF, 0)

// MOV copies the contents of one register to another register.
OPCODE(MOV, REG, DREG,  0)
