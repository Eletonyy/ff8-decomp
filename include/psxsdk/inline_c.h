#ifndef INLINE_C_H
#define INLINE_C_H

/**
 * @file
 * @brief GTE inline macros from PsyQ INLINE_C.H that libgte.h does not carry.
 *
 * Spelled as the SDK spells them. The bare op uses the GAS encoding of the
 * instruction word, as libgte.h does for AVSZ3/AVSZ4; the SDK's own
 * ".word 0x0000117f" is for Sony's assembler.
 */

/* Load the 3x3 of a MATRIX into the light matrix (L11..L33). */
#define gte_SetLightMatrix(r0) __asm__ volatile (        \
    "lw     $12, 0( %0 );"                               \
    "lw     $13, 4( %0 );"                               \
    "ctc2   $12, $8;"                                    \
    "ctc2   $13, $9;"                                    \
    "lw     $12, 8( %0 );"                               \
    "lw     $13, 12( %0 );"                              \
    "lw     $14, 16( %0 );"                              \
    "ctc2   $12, $10;"                                   \
    "ctc2   $13, $11;"                                   \
    "ctc2   $14, $12"                                    \
    :                                                    \
    : "r"(r0)                                            \
    : "$12", "$13", "$14")

/* Load the background colour (RBK, GBK, BBK). */
#define gte_ldbkdir(r0, r1, r2) __asm__ volatile (       \
    "ctc2   %0, $13;"                                    \
    "ctc2   %1, $14;"                                    \
    "ctc2   %2, $15"                                     \
    :                                                    \
    : "r"(r0), "r"(r1), "r"(r2))

/* NCLIP -- outer product of the three screen points, sign gives the winding. */
#define gte_nclip() __asm__ volatile (                   \
    "nop;"                                               \
    "nop;"                                               \
    ".word  0x4B400006"                                  \
    : : )

/* Store MAC0 (the NCLIP result). */
#define gte_stopz(r0) __asm__ volatile (                 \
    "swc2   $24, 0( %0 )"                                \
    :                                                    \
    : "r"(r0)                                            \
    : "memory")

/* Store SXY0..SXY2 straight into a POLY_GT3's three vertices. */
#define gte_stsxy3_gt3(r0) __asm__ volatile (            \
    "swc2   $12, 8( %0 );"                               \
    "swc2   $13, 20( %0 );"                              \
    "swc2   $14, 32( %0 )"                               \
    :                                                    \
    : "r"(r0)                                            \
    : "memory")

/* Store SXY0..SXY2 straight into a POLY_FT3's three vertices. */
#define gte_stsxy3_ft3(r0) __asm__ volatile (            \
    "swc2   $12, 8( %0 );"                               \
    "swc2   $13, 16( %0 );"                              \
    "swc2   $14, 24( %0 )"                               \
    :                                                    \
    : "r"(r0)                                            \
    : "memory")

/* RT -- rotation matrix times V0, plus the translation vector, unscaled. */
#define gte_rt() gte_mvmva(1, 0, 0, 0, 0)

#endif /* INLINE_C_H */
