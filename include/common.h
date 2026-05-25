#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;

#ifndef NULL
#define NULL ((void *)0)
#endif

/**
 * @brief 16-bit slot accessed as either a halfword or its two constituent bytes.
 *
 * Used to model struct fields that are read/written at different widths by
 * different functions — e.g. a u16 counter that one function updates via a
 * halfword store (@c sh) and another treats as two independent u8 flags via
 * byte stores (@c sb). Access the full halfword as @c .hword, the low byte
 * (little-endian @c byte[0]) as @c .b.lo, and the high byte (@c byte[1]) as
 * @c .b.hi.
 */
typedef union {
    u16 hword;
    struct { u8 lo; u8 hi; } b;
} U16Split;

/* No-op barrier to control register allocation for decomp matching.
   Compiles to nothing; safe to remove if byte-matching is not needed. */
#ifdef __GNUC__
#define REGALLOC_BARRIER(x) asm("" : "+r"(x))
#else
#define REGALLOC_BARRIER(x)
#endif
#define NOP() asm("nop")


/* Clamp value to [lo, hi] range. */
#define CLAMP(val, lo, hi) ((val) < (lo) ? (lo) : (val) > (hi) ? (hi) : (val))

/* Absolute value. Uses `<=` (rather than `<`) on the sign test so the
 * compiler emits `blez` for the branch, matching the original codegen
 * (`x == 0` falls into the negation arm, which still yields 0). */
#define abs(x) ((x) <= 0 ? -(x) : (x))

/*
 * Set/clear bit @c bitPos in a 64-bit flag set stored as @c u32 flags[2].
 * @c bitPos must be a signed type (s8 / s32) — this lets gcc 2.8.0 compile
 * the slot index `(bitPos > 0x1F)` as `sll, lui, slt` (without sra), matching
 * the natural codegen for signed-byte comparisons.
 */
#define SET_FLAG(flags, bitPos)   ((flags)[(bitPos) > 0x1F] |=  (1U << ((bitPos) & 0x1F)))
#define CLEAR_FLAG(flags, bitPos) ((flags)[(bitPos) > 0x1F] &= ~(1U << ((bitPos) & 0x1F)))

/* Keep a variable alive to prevent the compiler from optimizing it away. */
#define KEEP_ALIVE(x) asm volatile("" :: "r"(x))

/*
 * GP stack allocation — allocates `size` bytes from the GP-relative area,
 * returning the current GP in `ptr`. Used for temporary scratchpad structs.
 */
#define GP_ALLOC(ptr, size) \
    asm volatile("addu %0, $gp, $zero" : "=r"(ptr)); \
    asm volatile("addi $gp, $gp, %0" : : "i"(size))

/* Free `size` bytes from the GP stack (reverses GP_ALLOC). */
#define GP_FREE(size) \
    asm volatile("addi $gp, $gp, -%0" : : "i"(size))

/*
 * Combined GP save+set scratchpad macro — saves $gp to `saved`, sets $gp to
 * scratchpad (0x1F800300) via $a2. Used by btl_anim display list functions.
 */
#define GP_SAVE_SCRATCH(saved) \
    asm volatile("addu %0, $gp, $zero" : "=r"(saved)); \
    asm volatile("lui $a2, 0x1F80"); \
    asm volatile("ori $a2, $a2, 0x0300"); \
    asm volatile("addu $gp, $a2, $zero")

/*
 * Combined GP get return + restore macro — captures $gp (scratchpad pointer)
 * into ret, then restores original $gp from `saved`.
 */
#define GP_RESTORE_RET(saved, ret) \
    asm volatile( \
        "addu %0, $gp, $zero\n\t" \
        "addu $gp, %1, $zero" \
        : "=r"(ret) : "r"(saved))

/*
 * Hand-tuned addPrim variant — the 4-instruction sll/lwl/swl/swl
 * sequence that swaps the 24-bit P_TAG.addr field while leaving the
 * adjacent len byte untouched. Equivalent semantics to PsyQ's standard
 * addPrim(ot, p), but 4 instructions instead of 6+.
 *
 * Universal asm shape (identical across all 94 occurrences in the FF8
 * binary):
 *   sll $at, prim, 8
 *   lwl tmp, 0x2(ot)
 *   swl $at, 0x2(ot)
 *   swl tmp, 0x2(prim)
 *
 * Only `tmp` varies — gcc picks it based on which registers are alive at
 * the call site. We can't reconcile all 94 sites into a single macro:
 * even within one function multiple addPrim calls pick different temps,
 * and there's no single clobber list that produces every observed
 * choice. Instead we expose several variants whose name describes which
 * caller-saved regs they tell gcc to avoid for the asm output. Pick the
 * variant that matches the target's chosen temp register at each site.
 *
 * Observed temp-register distribution across the binary (see
 * docs/addprim-sites.md for the full per-site map):
 *   0× $v0 — always avoided; suggests $v0 was clobbered everywhere.
 *   2× $v1 — only when $v1 was genuinely free (early in the function).
 *   majority — $a3, $t0..$t9, $s0..$s7 depending on local pressure.
 *
 * If a new call site lands on a temp the variants below don't cover,
 * add a new variant rather than parameterising — the explicit name at
 * the call site documents the register-allocation contract.
 */

/* $v0 only — gcc will pick $v1 next. Use at sites where the target's
 * temp is $v1 (rare, see bc_object16/func_800CF418 site 1 and
 * bc_object21/func_800DD80C). */
#define addPrimFastNoV0(ot, p) do {                      \
    u32 _saved;                                          \
    __asm__ __volatile__(                                \
        ".set push\n"                                    \
        ".set noat\n"                                    \
        "sll $at, %2, 8\n"                               \
        "lwl %0, 2(%1)\n"                                \
        "swl $at, 2(%1)\n"                               \
        "swl %0, 2(%2)\n"                                \
        ".set pop\n"                                     \
        : "=&r"(_saved)                                  \
        : "r"(ot), "r"(p)                                \
        : "v0", "memory");                               \
} while (0)

/* $v0 and $v1 — gcc walks to the next free register, usually $a3 or a
 * caller-saved temp depending on what else is live. Most common shape
 * for tiny "first-arg" leaf functions (e.g. func_8003334C,
 * func_800CF2D0). */
#define addPrimFastNoV0V1(ot, p) do {                    \
    u32 _saved;                                          \
    __asm__ __volatile__(                                \
        ".set push\n"                                    \
        ".set noat\n"                                    \
        "sll $at, %2, 8\n"                               \
        "lwl %0, 2(%1)\n"                                \
        "swl $at, 2(%1)\n"                               \
        "swl %0, 2(%2)\n"                                \
        ".set pop\n"                                     \
        : "=&r"(_saved)                                  \
        : "r"(ot), "r"(p)                                \
        : "v0", "v1", "memory");                         \
} while (0)

#endif /* COMMON_H */
