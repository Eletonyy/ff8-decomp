#ifndef MENUJNC2_H
#define MENUJNC2_H

#include "common.h"
#include "menu.h"
#include "battle.h"
#include "ability_list.h"

/* Public prototypes (junction-menu entry points + magic-list callback). */
extern void junctionMenuUpdate();
extern void renderJunctionMenu();
extern s32 renderMagicItemCallback();

/* Private typedefs/structs (menujnc2-internal layout descriptors). */
/** @brief Stat-table layout entry: grid cell + label string ID (stride 8). */
typedef struct {
    /* 0x00 */ u8 col;        /**< Grid column index. */
    /* 0x01 */ u8 row;        /**< Grid row index. */
    /* 0x02 */ u16 statOffset;/**< Byte offset of the stat within char data (panel B). */
    /* 0x04 */ u16 labelId;   /**< Stat label string ID. */
    /* 0x06 */ u8 pad_06[2];
} StatTableEntry; /* 0x8 bytes */


/*
 * JunctionMenuEntry.availFlags — one bit per junction-ability availability.
 * (The x2/x4 tier ordering within each defense group is inferred, not confirmed.)
 */
#define JNC_AVAIL_ELEM_ATK       0x200    /* bit 9  — Elem-Atk-J */
#define JNC_AVAIL_STATUS_ATK     0x400    /* bit 10 — ST-Atk-J */
#define JNC_AVAIL_ELEM_DEF       0x800    /* bit 11 — Elem-Def-J */
#define JNC_AVAIL_STATUS_DEF     0x1000   /* bit 12 — ST-Def-J */
#define JNC_AVAIL_ELEM_DEF_X2    0x2000   /* bit 13 — Elem-Def-Jx2 */
#define JNC_AVAIL_ELEM_DEF_X4    0x4000   /* bit 14 — Elem-Def-Jx4 */
#define JNC_AVAIL_STATUS_DEF_X2  0x8000   /* bit 15 — ST-Def-Jx2 */
#define JNC_AVAIL_STATUS_DEF_X4  0x10000  /* bit 16 — ST-Def-Jx4 */

/* Stat-junction availability bits (JunctionType position) used by the encoder. */
#define JNC_AVAIL_HIT            0x80     /* bit 7  (JUNCTION_HIT) */
#define JNC_AVAIL_LCK            0x100    /* bit 8  (JUNCTION_LCK) */

/*
 * BattleCharData.abilityFlags source bits that encodeBattleAbilityFlags maps
 * into the junction-availability format above (source positions inferred from
 * that mapping).
 */
#define BTL_ABL_HIT_J           0x1
#define BTL_ABL_LCK_J           0x4
#define BTL_ABL_ELEM_ATK_J      0x8
#define BTL_ABL_ST_ATK_J        0x200
#define BTL_ABL_ELEM_DEF_J      0x4000
#define BTL_ABL_ST_DEF_J        0x8000

/*
 * menujnc2 overlay data (rodata/bss, 0x801EExxx). No dual-named
 * aliases remain.
 */
/** @brief Auto-junction priority tables (Atk/Mag/Def), each a 0xFF-terminated slot type list. */
extern u8 *g_autoJunctionPriority[];
extern u8 D_801EEAC0[];
extern JunctionMenuEntry g_junctionChars[];
extern JunctionGfEntry g_junctionGfTable[];
extern u8 g_junctionBackup[20];
extern s16 D_801EEB28[];
extern BattleCharData g_junctionPreview;
extern u8 g_junctionMenuActive;
extern s32 g_assignedAbilities[];
extern s32 g_availableAbilities[];
extern u8 D_801EEF10[];
extern u8 D_801EEF38;
extern JunctionSlotDetail D_801EEAD4[]; /**< Junction slot-detail table (overlay rodata, 9 entries HP..LCK). */
extern u8 D_801EEF40[];
extern u8 D_801EEF9A;
extern u8 D_801EEED0[];
extern s16 D_801EEB20;
extern s16 D_801EEB22[];
extern s16 D_801EEB30[];
extern s16 D_801EEB38[];
extern s16 D_801EEB1C[];
extern JunctionGfEntry D_801EEDD0;
extern u32 D_801EEFC0[];
extern s32 D_801EED00;
extern AbilityListEntry D_801EEC50[];
extern u8 D_801EEDE0[];
extern StatTableEntry D_801EEBA8[];          /**< Panel-A stat-table layout (13 entries). */
extern StatTableEntry D_801EEB40[];          /**< Panel-B stat-table layout (13 entries). */
extern StatTableEntry D_801EEC10[];          /**< Panel-C stat-table layout (8 entries). */



/* Private prototypes (functions internal to menujnc2.c). */
u8 *decodeMenuString(u8 *src, u8 *dst, s32 charIdx);
s32 autoJunctionSlot(s32 charIdx, MagicSlot *magicSlots, s32 slotType, s32 flagMask);
s32 renderJunctionSlots(s32 charIdx, s32 abilityList, s32 slotType, s32 pos);
void autoJunctionAll(s32 charIdx, s32 tableIdx);
void updateJunctionSlotCount(JunctionMenuCtx *ctx);
void stashCharacterJunctions(s32 charIdx);
void restoreCharacterJunctions(s32 charIdx);
void assignJunctionSlot(s32 charIdx, s32 slotIndex, s32 mode, s32 selection, s32 doWrite);
void buildAssignedAbilities(s32 charIdx);
void buildAvailableAbilities(s32 charIdx);
void buildAbilityTables(s32 charIdx);
void renderMagicListEntry(s32 renderCtx, s32 row);
void renderStatColumnEntry(s32 renderCtx, s32 row, s32 widthOffset);
void renderStatListEntry(s32 renderCtx, s32 slotIdx);
s32 getJunctionSlotFlags(s32 charIdx, s32 slotOffset);
void renderAbilityEntry(s32 renderCtx, s32 index);
s32 getJunctionSlotCount(s32 charIdx, s32 slotType);
s32 buildMagicAvailMask(s32 charIdx, s32 slotOffset);
s32 getAbilityScrollOffset(s32 index);
s32 renderInnerPanel(s32 pos);
s32 renderInnerPanelAlt(s32 pos);
void validateCommandSlots(s32 charIdx);
void validateAbilitySlots(s32 charIdx);
void refreshJunctionState(s32 charIdx);
void syncCharacterHp(s32 charIdx);
void snapshotJunctionPreview(s32 charIdx);
void applyJunctedGfs(s32 charIdx);
void stashJunctedGfs(s32 charIdx);
void setJunctionHp(s32 charIdx, s32 hp);
void saveCommandAbilityBackup(s32 charIdx, s32 subSlot);
void restoreCommandAbilityBackup(s32 charIdx, s32 subSlot);
void revertJunctionState(s32 charIdx);
void initJunctionBackups(s32 charIdx);
void rebuildJunctionFlags(s32 charIdx);
void initJunctionChars(s32 mask);
void renderStatEffectBar(s32 renderCtx, JunctionMenuCtx *ctx);
void renderStatDeltaEntry(JunctionMenuCtx *ctx, s32 renderCtx, s32 column);
void renderStatValueBar(JunctionMenuCtx *ctx, s32 renderCtx, s32 column);
s32 junctionGfToChar(s32 charIdx, s32 gfIdx);
void compactCommandSlots(s32 charIdx);
void compactAbilitySlots(s32 charIdx);
s32 unjunctionGf(s32 charIdx, s32 gfIdx);
s32 unjunctionGfAndRefresh(s32 charIdx, s32 gfIdx);
void previewJunctionChange(s32 charIdx, s32 gfIdx, s32 slot, s32 abilityId);
s32 getAbilityNamePtr(s32 type, s32 index);
s32 getJunctionCapabilities(s32 charIdx);
void buildMagicLookupTable(s32 charIdx);
s32 encodeBattleAbilityFlags(BattleCharData *charData);
void renderStatTableA(s32 renderCtx, s32 cursorY, s32 xBase, s32 yBase);
void renderStatTableB(s32 renderCtx, s32 cursorY, s32 xBase, s32 yBase);
void renderStatTableC(s32 renderCtx, s32 cursorY, s32 xBase, s32 yBase);
void renderStatTableD(s32 renderCtx, s32 cursorY, s32 xBase, s32 yBase);
void renderStatGrid(s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 renderStatDeltaBar(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 renderStatDeltaBarExt(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
void setupMagicListPanel(JunctionMenuCtx *ctx, s32 renderCtx, s32 callbackParam, s16 x, s16 y);
void renderGfMagicGrid(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 xBase, s32 yBase);
s32 renderGfMagicEntry(s32 renderCtx, s32 cursorY, s32 col, s32 row, s32 xOff);
void renderGfMagicPanel(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 checkJunctionCompat(s32 currentMask, s32 availMask, s32 abilityBit);
s32 renderHpJunctionSlot(s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 charIdx, s32 gfIdx);
s32 renderStatusDefSlot(s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 charIdx, s32 gfIdx);
s32 renderElemAtkSlot(s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 charIdx, s32 gfIdx);
s32 renderElemDefSlot(s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 charIdx, s32 gfIdx);
s32 renderElemJunctionPanel(s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 charIdx, s32 gfIdx);
s32 renderStatusJunctionPanel(s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 charIdx, s32 gfIdx);
s32 renderJunctionStatPanel(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 showGf);
void setupStatBorderPanel(s32 ctx, s32 mode, s32 x, s32 y, s32 renderParam);
s32 renderJunctionHeader(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
void renderJunctionComposite(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y, s32 scale, s32 showGf);
s32 renderMagicJunctionEntry(s32 renderCtx, s32 cursorY, s32 col, s32 row, s32 xOff);
void renderMagicListPanel(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 renderAbilityListEntry(s32 ctx, s32 cursorY, s32 row, s32 col, s32 panelX);
void renderAbilityListPanel(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 renderStatRowGrid(u8 *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 renderGfCompatGrid(u8 *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
void renderCharSwitchPanel(JunctionMenuCtx *ctx, s32 renderCtx, s32 cursorY, s32 x, s32 y);
s32 renderCharNameBar(s32 renderCtx, s32 cursorY, s32 x, s32 height, s32 charIdx);
void initJunctionGfTable(void);
void initJunctionMenu(MenuParentCtx *parentCtx);
void enterJunctionMenu(MenuParentCtx *parentCtx);
void resetJunctionMenu(MenuParentCtx *parentCtx);

#endif /* MENUJNC2_H */
