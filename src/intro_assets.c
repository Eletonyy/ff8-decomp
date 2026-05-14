/**
 * @file intro_assets.c
 * @brief CD asset index for the intro overlay.
 *
 * Each entry is a @c (sector, size) pair pointing at an LZSS-compressed
 * bitmap on the disc. After @c cdReadAsyncSync (mode 7, LZSS-decoded by
 * @c func_80039520) decompresses one into staging RAM:
 *
 *   - 0x80100000: 8-byte header (@c u16 @c hdr0, @c hdr1, then @c width and
 *     @c height as little-endian @c u16).
 *   - 0x80100008: raw 16bpp BGR555 pixels — 640×400 for entries fed to
 *     @c func_80098378 mode 0, 580×406 for mode 1.
 *
 * @c func_80098338 indexes this table by stage; @c loadWrongDiscWarning reads
 * entry 34 directly (offset 0x110 = 34 * 8) and loads it to a separate
 * staging area at @c 0x8017D000 for the mode-2 flash overlay.
 *
 * To dump the actual images from a disc image use
 * @c tools/extract_intro_images.py.
 *
 * In a CD-mastering pipeline this file would be regenerated from the
 * disc layout; for now the offsets are copied from the original ROM.
 */
#include "intro_assets.h"

u32 g_introAssetTable[] = {
    /* idx  sector       size           description (verified by decoding)   */
    0x000246D9, 0x0002E5F2,  /*  0: 580x406  disc-swap prompt for Disc 1      */
    0x00024736, 0x0002CE46,  /*  1: 580x406  disc-swap prompt for Disc 2      */
    0x00024790, 0x0002D772,  /*  2: 580x406  disc-swap prompt for Disc 3      */
    0x000247EB, 0x0002C834,  /*  3: 580x406  disc-swap prompt for Disc 4      */
    0x00024845, 0x0000F199,  /*  4: 640x400  Squaresoft logo                  */
    0x00024864, 0x0000FCBC,  /*  5: 640x400  intro slide 5                    */
    0x00024884, 0x0000F96C,  /*  6: 640x400  intro slide 6                    */
    0x000248A4, 0x00024404,  /*  7: 640x400  intro slide 7 (large/full image) */
    0x000248ED, 0x00029BC7,  /*  8: 640x400  intro slide 8 (large/full image) */
    0x00024941, 0x0000FC47,  /*  9: 640x400  intro slide 9                    */
    0x00024961, 0x00010186,  /* 10: 640x400  intro slide 10                   */
    0x00024982, 0x00023D6F,  /* 11: 640x400  intro slide 11 (large/full)      */
    0x000249CA, 0x000221CF,  /* 12: 640x400  intro slide 12 (large/full)      */
    0x00024A0F, 0x0000FCA1,  /* 13: 640x400  intro slide 13                   */
    0x00024A2F, 0x0000FF14,  /* 14: 640x400  intro slide 14                   */
    0x00024A4F, 0x00020597,  /* 15: 640x400  intro slide 15 (large/full)      */
    0x00024A90, 0x00021378,  /* 16: 640x400  intro slide 16 (large/full)      */
    0x00024AD3, 0x00010261,  /* 17: 640x400  intro slide 17                   */
    0x00024AF4, 0x0000FF48,  /* 18: 640x400  intro slide 18                   */
    0x00024B14, 0x0002144C,  /* 19: 640x400  intro slide 19 (large/full)      */
    0x00024B57, 0x00026CAD,  /* 20: 640x400  intro slide 20 (large/full)      */
    0x00024BA5, 0x00010236,  /* 21: 640x400  intro slide 21                   */
    0x00024BC6, 0x000107D9,  /* 22: 640x400  intro slide 22                   */
    0x00024BE7, 0x00029B94,  /* 23: 640x400  intro slide 23 (large/full)      */
    0x00024C3B, 0x000279E1,  /* 24: 640x400  intro slide 24 (large/full)      */
    0x00024C8B, 0x00010054,  /* 25: 640x400  intro slide 25                   */
    0x00024CAC, 0x0000FFB0,  /* 26: 640x400  intro slide 26                   */
    0x00024CCC, 0x0001FE02,  /* 27: 640x400  intro slide 27 (large/full)      */
    0x00024D0C, 0x00026F37,  /* 28: 640x400  intro slide 28 (large/full)      */
    0x00024D5A, 0x0001007E,  /* 29: 640x400  intro slide 29                   */
    0x00024D7B, 0x0000FBD7,  /* 30: 640x400  intro slide 30                   */
    0x00024D9B, 0x00026F1F,  /* 31: 640x400  intro slide 31 (large/full)      */
    0x00024DE9, 0x0002D5DA,  /* 32: 640x400  "The End" title card             */
    0x00024E44, 0x00015485,  /* 33: 640x400  "Final Fantasy VIII" logo        */
    0x00024E6F, 0x0000FF09,  /* 34: 580x406  "CAUTION WRONG DISC" warning     */
    0x00024E8F, 0x0000F131,  /* 35: 640x400  "Published by Square Electronic Arts L.L.C" attribution */
};
