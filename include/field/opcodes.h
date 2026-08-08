/**
 * @file opcodes.h
 * @brief Field script-VM opcode dispatch table, owned by @c src/field/opcodes.c.
 *
 * The table holds 392 handler pointers (0x620 bytes at @c 0x800C6760):
 * indices @c 0x000-0x011 are the stack-arithmetic sub-table reached through
 * the @c CAL opcode, and @c 0x012-0x187 are the main field-VM opcodes.
 *
 * The runtime dispatcher indexes 0x48 bytes into the table (that is
 * @ref FIELD_OPCODE_BASE entries), so wiki opcode @c N is table index
 * @c N + @ref FIELD_OPCODE_BASE here.
 */
#ifndef FIELD_OPCODES_H
#define FIELD_OPCODES_H

#include "common.h"

/**
 * @brief Opcode handler pointer.
 *
 * Declared without a prototype on purpose: handlers take differing argument
 * counts and types (@c Actor* alone, or @c Actor* plus an arg), and the table
 * casts each entry to this type.
 */
typedef s32 (*OpcodeFn)();

/**
 * @brief Index of the first main-VM opcode in @ref g_fieldOpcodeTable.
 *
 * Entries below this are the @c CAL stack-arithmetic sub-table, so the
 * bytecode dispatcher looks up opcode @c N at
 * @c g_fieldOpcodeTable[N + FIELD_OPCODE_BASE].
 */
#define FIELD_OPCODE_BASE 0x12

/** @brief The 392-entry field script-VM dispatch table. */
extern s32 (*g_fieldOpcodeTable[392])();

#endif /* FIELD_OPCODES_H */
