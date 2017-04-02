// HALT stops execution of the VM.
OPCODE(HALT)

// LOADI loads an immediate out of the instruction stream into a register.
// Arguments: register(1), immediate(8, 16, 32, 64)
OPCODE(LOADI_8)
OPCODE(LOADI_16)
OPCODE(LOADI_32)
OPCODE(LOADI_64)
