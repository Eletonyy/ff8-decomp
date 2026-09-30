#ifndef BTL_ENTITY_H
#define BTL_ENTITY_H

#include "common.h"
#include "battle.h"

extern s32 *getEntityTablePtr(s32 idx);
extern u8 *func_8002BF24(u32 *ot, u8 *head);
extern s32  func_8002C734(s32 c);
extern void dispatchBattleEntity(s32 idx);
extern s32  allocBattleEntitySlot(void);
extern void initAllBattleEntities(void);
extern void setBattleEntityBase(s32 val);
extern s32  getMaxBattleEntities(void);
extern u8   getDigitBaseCode(void);
extern void setDigitBaseCode(u8 val);
extern void setDialogTextOrigin(s32 idx, s32 x, s32 y);
extern void setDialogChoices(s32 idx, s32 first, s32 last, s32 cancel);
extern void setDialogChoiceCursor(s32 idx, s32 val);
extern void setDialogDrawCallback(s32 idx, DialogDrawCallback val);
extern void setDialogUpdateCallback(s32 idx, DialogCallback val);
extern void setDialogBrightness(s32 idx, s32 val);

/**
 * @brief Emit one sprite per glyph of @p str and link them into @p ot.
 *
 * Walks the string, looks each character up in
 * @c g_iconTable, and writes a 0x14-byte sprite packet per atlas cell into the
 * primitive buffer, adding each to @p ot. Characters below 0x19 are skipped;
 * 0x19..0x1F introduce a two-byte code. The run is closed with a draw-mode
 * primitive, and the pen advances 8 pixels per character.
 *
 * @param ot   Ordering table the sprites are linked into.
 * @param prim Primitive buffer cursor.
 * @param x    Pen X, advanced per character.
 * @param y    Pen Y. Nothing is drawn outside -8 .. 0x100.
 * @param str  NUL-terminated string in the game's own encoding.
 * @param row  Atlas row; becomes the texture page/CLUT field of each sprite.
 * @return The primitive cursor past the last packet written.
 */
void *func_8002C56C(void *ot, void *prim, s32 x, s32 y, u8 *str, s32 row);

#endif
