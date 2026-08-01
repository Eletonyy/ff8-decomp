#ifndef LIBGTE_H
#define LIBGTE_H

#include "common.h"

#define ONE 4096 /**< GTE fixed-point 1.0 (12-bit fractional). */

/** @brief 3x3 rotation matrix with translation vector (32 bytes). */
typedef struct {
    s16 m[3][3];    /**< 3x3 rotation matrix (fixed point). */
    s32 t[3];       /**< Translation vector (tx, ty, tz). */
} MATRIX;

/** @brief Long word 3D vector. */
typedef struct {
    s32 vx, vy;
    s32 vz, pad;
} VECTOR;

/** @brief Short word 3D vector. */
typedef struct {
    s16 vx, vy;
    s16 vz, pad;
} SVECTOR;

/** @brief Color vector. */
typedef struct {
    u8 r, g, b, cd;
} CVECTOR;

/** @brief 2D short vector. */
typedef struct {
    s16 vx, vy;
} DVECTOR;

/* --- GTE matrix operations (signatures match PsyQ 4.6 LIBGTE.H) --- */

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *scale);
MATRIX *ScaleMatrixL(MATRIX *m, VECTOR *scale);
MATRIX *TransposeMatrix(MATRIX *m0, MATRIX *m1);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetLightMatrix(MATRIX *m);
void SetColorMatrix(MATRIX *m);
void SetBackColor(s32 rbk, s32 gbk, s32 bbk);
void SetFarColor(s32 rfc, s32 gfc, s32 bfc);

/* --- GTE transform operations --- */

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
s32 RotTransPers(SVECTOR *v0, s32 *sxy, s32 *p, s32 *flag);
/** @brief The main binary's own @c RotTransPers copy; overlays link it by address, so it
 *         keeps its @c func_ name. */
s32 func_80040DE4(SVECTOR *v0, s32 *sxy, s32 *p, s32 *flag);
s32 RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3,
                  s32 *sxy0, s32 *sxy1, s32 *sxy2, s32 *sxy3,
                  s32 *p, s32 *flag);
s32 NormalClip(s32 sxy0, s32 sxy1, s32 sxy2);

/* --- GTE color/lighting --- */

void DpqColor(CVECTOR *v0, s32 p, CVECTOR *v1);

/* --- GTE initialization --- */

void InitGeom(void);
void SetGeomOffset(s32 ofx, s32 ofy);   /**< Set GTE screen offset OFX/OFY (signed, scaled <<16). */
void SetGeomScreen(s32 h);   /**< Set GTE projection-plane distance H (screen depth). */

/* --- GTE math --- */

s32 SquareRoot0(s32 a);
s32 SquareRoot12(s32 a);   /**< Square root of a 20.12 fixed-point value. */
s32 VectorNormal(VECTOR *v0, VECTOR *v1);   /**< Normalise @p v0 into @p v1; returns the squared length. */
s32 rsin(s32 a);
s32 rcos(s32 a);

/* --- Inline GTE (COP2) opcode macros ---
 *
 * These expand to @c __asm__ volatile sequences that emit raw COP2 opcodes,
 * so the caller can pipeline multiple GTE ops with the matching delay slots
 * that @c cc1 schedules. Equivalent to PsyQ's @c inline_c.h but re-encoded
 * for GAS: where the PsyQ SDK used assembler-side rewrites of bogus
 * @c 0x000013bf -style constants, we emit the real @c 0x4A... COP2 bytes
 * (same convention as sotn-decomp's @c gte.inc).
 */

#define gte_SetRotMatrix( r0 ) __asm__ volatile (        \
    "lw     $12, 0( %0 );"                               \
    "lw     $13, 4( %0 );"                               \
    "ctc2   $12, $0;"                                    \
    "ctc2   $13, $1;"                                    \
    "lw     $12, 8( %0 );"                               \
    "lw     $13, 12( %0 );"                              \
    "lw     $14, 16( %0 );"                              \
    "ctc2   $12, $2;"                                    \
    "ctc2   $13, $3;"                                    \
    "ctc2   $14, $4"                                     \
    :                                                    \
    : "r"( r0 )                                          \
    : "$12", "$13", "$14" )

#define gte_SetTransMatrix( r0 ) __asm__ volatile (      \
    "lw     $12, 20( %0 );"                              \
    "lw     $13, 24( %0 );"                              \
    "ctc2   $12, $5;"                                    \
    "lw     $14, 28( %0 );"                              \
    "ctc2   $13, $6;"                                    \
    "ctc2   $14, $7"                                     \
    :                                                    \
    : "r"( r0 )                                          \
    : "$12", "$13", "$14" )

#define gte_SetTransVector( r0 ) __asm__ volatile (      \
    "lw     $12, 0( %0 );"                               \
    "lw     $13, 4( %0 );"                               \
    "lw     $14, 8( %0 );"                               \
    "ctc2   $12, $5;"                                    \
    "ctc2   $13, $6;"                                    \
    "ctc2   $14, $7"                                     \
    :                                                    \
    : "r"( r0 )                                          \
    : "$12", "$13", "$14" )

#define gte_ldv0( r0 ) __asm__ volatile (                \
    "lwc2   $0, 0( %0 );"                                \
    "lwc2   $1, 4( %0 )"                                 \
    :                                                    \
    : "r"( r0 ) )

/* Load short vector — fills IR1/IR2/IR3 from the three s16 components of
 * the SVECTOR @p r0 (mtc2 to GTE data regs $9/$10/$11). */
#define gte_ldsv( r0 ) __asm__ volatile (                \
    "lhu    $12, 0( %0 );"                               \
    "lhu    $13, 2( %0 );"                               \
    "lhu    $14, 4( %0 );"                               \
    "mtc2   $12, $9;"                                    \
    "mtc2   $13, $10;"                                   \
    "mtc2   $14, $11"                                    \
    :                                                    \
    : "r"( r0 )                                          \
    : "$12", "$13", "$14" )

/* SQR — square IR1/IR2/IR3 into MAC1/MAC2/MAC3 (and back to IR via the
 * limiter). @p sf is the shift fraction (0 = full precision, 1 = >>12).
 * Encoding: COP2 funct 0x28 with sf at bit 19. Two nops cover the GTE
 * data-reg load stall before SQR can read the IR registers. */
#define gte_SQR( sf ) __asm__ volatile (                 \
    "nop;"                                               \
    "nop;"                                               \
    ".word  %0"                                          \
    :                                                    \
    : "g"( 0x4AA00428 | ((sf) << 19) ) )

#define gte_ldtr( x, y, z ) __asm__ volatile (           \
    "ctc2   %0, $5;"                                     \
    "ctc2   %1, $6;"                                     \
    "ctc2   %2, $7"                                      \
    :                                                    \
    : "r"( x ), "r"( y ), "r"( z ) )

/* Load the first OP (outer/cross product) operand — the three words of the
 * VECTOR @p r0 go into the rotation-matrix diagonal R11/R22/R33, which OP
 * reads as its D1/D2/D3 vector. */
#define gte_ldopv1( r0 ) __asm__ volatile (              \
    "lw     $12, 0( %0 );"                               \
    "lw     $13, 4( %0 );"                               \
    "ctc2   $12, $0;"                                    \
    "lw     $14, 8( %0 );"                               \
    "ctc2   $13, $2;"                                    \
    "ctc2   $14, $4"                                     \
    :                                                    \
    : "r"( r0 )                                          \
    : "$12", "$13", "$14" )

/* Load the second OP operand — the three words of the VECTOR @p r0 go
 * straight into IR1/IR2/IR3. */
#define gte_ldopv2( r0 ) __asm__ volatile (              \
    "lwc2   $11, 8( %0 );"                               \
    "lwc2   $9, 0( %0 );"                                \
    "lwc2   $10, 4( %0 )"                                \
    :                                                    \
    : "r"( r0 ) )

#define gte_stlvnl( r0 ) __asm__ volatile (              \
    "swc2   $25, 0( %0 );"                               \
    "swc2   $26, 4( %0 );"                               \
    "swc2   $27, 8( %0 )"                                \
    :                                                    \
    : "r"( r0 )                                          \
    : "memory" )

#define gte_mvmva_core( r0 ) __asm__ volatile (          \
    "nop;"                                               \
    "nop;"                                               \
    ".word  %0"                                          \
    :                                                    \
    : "g"( r0 ) )

/* MVMVA: opcode COP2 (0x12), CO bit set, funct 0x12. Base 0x4A400012 is
 * (sf=0, mx=0, v=0, cv=0, lm=0); shift factor at bit 19, matrix at 17,
 * vector at 15, translation vector at 13, limit at 10. Matches the encoding
 * used by sotn-decomp's @c MVMVA macro in @c include/gte.inc. */
#define gte_mvmva( sf, mx, v, cv, lm ) gte_mvmva_core(   \
    0x4A400012 | ((sf) << 19) | ((mx) << 17)             \
               | ((v)  << 15) | ((cv) << 13)             \
               | ((lm) << 10) )

/* Load V0/V1/V2 simultaneously from 3 separate SVECTORs. */
#define gte_ldv3( r0, r1, r2 ) __asm__ volatile (        \
    "lwc2   $0,  0( %0 );"                               \
    "lwc2   $1,  4( %0 );"                               \
    "lwc2   $2,  0( %1 );"                               \
    "lwc2   $3,  4( %1 );"                               \
    "lwc2   $4,  0( %2 );"                               \
    "lwc2   $5,  4( %2 )"                                \
    :                                                    \
    : "r"( r0 ), "r"( r1 ), "r"( r2 ) )

/* Load V0/V1/V2 simultaneously from 3 consecutive SVECTORs (24 bytes). */
#define gte_ldv3c( r0 ) __asm__ volatile (               \
    "lwc2   $0,  0( %0 );"                               \
    "lwc2   $1,  4( %0 );"                               \
    "lwc2   $2,  8( %0 );"                               \
    "lwc2   $3, 12( %0 );"                               \
    "lwc2   $4, 16( %0 );"                               \
    "lwc2   $5, 20( %0 )"                                \
    :                                                    \
    : "r"( r0 ) )

/* Store IR1/IR2/IR3 as a 3-component s16 vector (mfc2 + sh). */
#define gte_stsv( r0 ) __asm__ volatile (                \
    "mfc2   $12, $9;"                                    \
    "mfc2   $13, $10;"                                   \
    "mfc2   $14, $11;"                                   \
    "sh     $12, 0( %0 );"                               \
    "sh     $13, 2( %0 );"                               \
    "sh     $14, 4( %0 )"                                \
    :                                                    \
    : "r"( r0 )                                          \
    : "$12", "$13", "$14", "memory" )

/* Store SXY0/SXY1/SXY2 to 3 separate destinations. */
#define gte_stsxy3( r0, r1, r2 ) __asm__ volatile (      \
    "swc2   $12, 0( %0 );"                               \
    "swc2   $13, 0( %1 );"                               \
    "swc2   $14, 0( %2 )"                                \
    :                                                    \
    : "r"( r0 ), "r"( r1 ), "r"( r2 )                    \
    : "memory" )

/* Store SXY0/SXY1/SXY2 to 3 consecutive words. */
#define gte_stsxy3c( r0 ) __asm__ volatile (             \
    "swc2   $12, 0( %0 );"                               \
    "swc2   $13, 4( %0 );"                               \
    "swc2   $14, 8( %0 )"                                \
    :                                                    \
    : "r"( r0 )                                          \
    : "memory" )

/* Store the single projected SXY from $14. */
#define gte_stsxy( r0 ) __asm__ volatile (               \
    "swc2   $14, 0( %0 )"                                \
    :                                                    \
    : "r"( r0 )                                          \
    : "memory" )

/* Store OTZ (depth) from $7. */
/* Store OTZ: SZ2 (GTE $19) scaled down by 4, the usual ordering-table index. */
#define gte_stszotz( r0 ) __asm__ volatile (             \
    "mfc2   $12, $19;"                                   \
    "nop;"                                               \
    "sra    $12, $12, 2;"                                \
    "sw     $12, 0( %0 )"                                \
    :                                                    \
    : "r"( r0 )                                          \
    : "memory" )

#define gte_stotz( r0 ) __asm__ volatile (               \
    "swc2   $7, 0( %0 )"                                 \
    :                                                    \
    : "r"( r0 )                                          \
    : "memory" )

/* Bare GTE ops (each emits 2 nops + the encoded instruction word). */
#define gte_RTPT() __asm__ volatile (                    \
    "nop;"                                               \
    "nop;"                                               \
    ".word  0x4A280030"                                  \
    : : )

#define gte_RTPS() __asm__ volatile (                    \
    "nop;"                                               \
    "nop;"                                               \
    ".word  0x4A180001"                                  \
    : : )

#define gte_AVSZ4() __asm__ volatile (                   \
    "nop;"                                               \
    "nop;"                                               \
    ".word  0x4B68002E"                                  \
    : : )

/* OP (sf = 0) — cross product of the vector loaded by @ref gte_ldopv1 with
 * the one loaded by @ref gte_ldopv2, full precision (no >>12), leaving the
 * result in MAC1..MAC3 for @ref gte_stlvnl. */
#define gte_OP0() __asm__ volatile (                     \
    "nop;"                                               \
    "nop;"                                               \
    ".word  0x4B70000C"                                  \
    : : )

#endif /* LIBGTE_H */
