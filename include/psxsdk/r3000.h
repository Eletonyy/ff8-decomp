#ifndef R3000_H
#define R3000_H

/**
 * @file
 * @brief Status register bits from PsyQ R3000.H, spelled as the SDK spells them.
 */

#define SR_CU2 0x40000000 /**< Coprocessor 2, the GTE, is usable. */
#define SR_IEP 0x00000004 /**< Previous interrupt enable: the current one is saved here when an exception is taken. */

#endif /* R3000_H */
