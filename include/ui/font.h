#ifndef UI_FONT_H
#define UI_FONT_H

#include "common.h"
#include "tim.h"

extern u8 getDigitBaseCode(void);
extern void setDigitBaseCode(u8 val);
extern void func_8002C130(void);
extern void func_8002C3AC(NameFont *font, s32 useTimPosition);

/**
 * @brief Emit one sprite per glyph of @p str and link them into @p ot.
 *
 * Walks the string, looks each character up in
 * @c g_iconTable, and writes a 0x14-byte sprite packet per atlas cell into the
 * primitive buffer, adding each to @p ot. Characters below 0x19 are skipped;
 * 0x19..0x1F introduce a two-byte code. The run is closed with a draw-mode
 * primitive, and the pen advances 8 pixels per character.
 *
 * @param ot Ordering table the sprites are linked into.
 * @param prim Primitive buffer cursor.
 * @param x Pen X, advanced per character.
 * @param y Pen Y. Nothing is drawn outside -8 .. 0x100.
 * @param str NUL-terminated string in the game's own encoding.
 * @param row Atlas row; becomes the texture page/CLUT field of each sprite.
 * @return The primitive cursor past the last packet written.
 */
void *func_8002C56C(void *ot, void *prim, s32 x, s32 y, u8 *str, s32 row);

extern s32 func_8002C734(s32 c);

#endif
