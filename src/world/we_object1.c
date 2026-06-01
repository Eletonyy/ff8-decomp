#include "common.h"
#include "cd.h"
#include "sound.h"
#include "world.h"
#include "psxsdk/libapi.h"
#include "psxsdk/libgpu.h"

extern u8 D_800780D8[];
extern u8 D_800C4DCC[];
extern u8 D_800C4FD3;
extern u8 D_800C4FD4;
extern u8 D_800C4FD5;
extern u8 D_800C4FD6;
extern u8 D_800C4FD7;
extern u8 D_800D23D8[];

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_800997E8);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_80099B48);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_80099C84);

extern SeqEntry D_800C4FD8[];

/**
 * @brief Kick off the SFX for @c D_800C4FD8[idx] and mark it active.
 *
 * Dispatches on the entry's @c bankHandle:
 *  - non-zero: @c sndPlayBankSfx(bankHandle, @c field04, @c field08,
 *    @c field0C).
 *  - zero:     @c sndPlaySfx(field10, @c field04, @c field08,
 *    @c field0C).
 *
 * Then sets @c field12 = @c 1 to mark the entry as running.
 *
 * @param idx Index into @c D_800C4FD8.
 */
void func_80099EDC(s32 idx) {
    if (D_800C4FD8[idx].bankHandle != 0) {
        sndPlayBankSfx(D_800C4FD8[idx].bankHandle,
                       D_800C4FD8[idx].field04,
                       D_800C4FD8[idx].field08,
                       D_800C4FD8[idx].field0C);
    } else {
        sndPlaySfx(D_800C4FD8[idx].field10,
                   D_800C4FD8[idx].field04,
                   D_800C4FD8[idx].field08,
                   D_800C4FD8[idx].field0C);
    }
    D_800C4FD8[idx].field12 = 1;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_80099F78);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A4DC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A638);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A7C0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009A954);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009AD3C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009AEE4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B358);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B550);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B748);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B840);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009B954);

/* Cached world-map scroll position, stored at HALF resolution (x, y). */
extern s32 D_800C4DA0;
extern s32 D_800C4DA4;

/**
 * @brief Snap the cached scroll position to the nearest candidate vector.
 *
 * Scans @p coords (an array of @p count 2D vectors) for the first whose (x, y)
 * lies within ±8 of the cached scroll position. That position
 * (@c D_800C4DA0 = x, @c D_800C4DA4 = y) is stored at half resolution, so it is
 * doubled for the comparison and the matched vector is halved on store. If no
 * candidate is close enough, the cache is left unchanged.
 *
 * @param coords Candidate 2D positions.
 * @param count  Number of candidates.
 */
void func_8009BFA0(DVECTOR *coords, s16 count) {
    DVECTOR *v = coords;
    s32 i;

    for (i = 0; i < count; i++, v++) {
        /* accept when |vx - x*2| < 8, tested per sign so it compiles to the
           original's two-branch (blez/slti) form rather than an abs() */
        s32 dx = v->vx - D_800C4DA0 * 2;
        if (dx > 0 ? (dx < 8) : (D_800C4DA0 * 2 - v->vx < 8)) {
            s32 dy = v->vy - D_800C4DA4 * 2;
            if (dy > 0 ? (dy < 8) : (D_800C4DA4 * 2 - v->vy < 8)) {
                D_800C4DA0 = (s16)v->vx >> 1;
                D_800C4DA4 = (s16)v->vy >> 1;
                return;
            }
        }
    }
    /* no-op that keeps coords live to the end, so the loop walker is held in a
       saved register — matches the original's register allocation */
    coords++;
    coords--;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C070);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C1A4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C294);

extern s32 func_8009CA34(s32 *src, ImageDesc *desc);
extern RECT D_800C8698;

/**
 * @brief Variant of @c func_8009C5FC that overrides the first
 *        @c RECT's @c (x, y) with caller-supplied coordinates.
 *
 * Same two-stage @c LoadImage flow as @c func_8009C5FC, but the first
 * blit's top-left corner is replaced by @p x, @p y (the @c w / @c h
 * still come from the descriptor). The second blit (when bit 3 of
 * @c desc.flag is set) uses @c desc.rect2 verbatim.
 *
 * Streams to the scratch @c D_800C8698 (separate from
 * @c func_8009C5FC's @c D_800C8640).
 *
 * @param src Source buffer fed to @c func_8009CA34.
 * @param x   Override X coordinate for the first blit.
 * @param y   Override Y coordinate for the first blit.
 */
void func_8009C478(s32 *src, s32 x, s32 y) {
    ImageDesc desc;
    func_8009CA34(src, &desc);
    D_800C8698.x = x;
    D_800C8698.y = y;
    D_800C8698.w = desc.rect1.w;
    D_800C8698.h = desc.rect1.h;
    LoadImage(&D_800C8698, desc.data1);
    if ((desc.flag >> 3) & 1) {
        D_800C8698.x = desc.rect2.x;
        D_800C8698.y = desc.rect2.y;
        D_800C8698.w = desc.rect2.w;
        D_800C8698.h = desc.rect2.h;
        LoadImage(&D_800C8698, desc.data2);
    }
}

/**
 * @brief Fatal-error thunk that invokes the PSX SDK SystemError with category 0x57.
 *
 * Forwards @p rc to SystemError with a fixed category code. Callers pass a
 * line-number-like token so the fault site can be identified post-mortem.
 *
 * @note Category values in use elsewhere: field → 0x53, battle
 *       → 0x59, world → 0x57. They happen to be ASCII ('S','Y','W')
 *       but the mapping to subsystem isn't obvious — purpose uncertain.
 *
 * @param rc Diagnostic code (typically a unique line/site identifier).
 */
void func_8009C528(s32 rc) {
    SystemError(0x57, rc);
}

extern void *memcpy(void *dst, const void *src, u32 n);
extern u8 D_800980CC[]; /* "x:\USPC\WORLD" — dev-filesystem prefix (13 chars + NUL) */

/**
 * @brief Rewrite an ISO9660 disc path into a dev-filesystem path.
 *
 * Copies @p src to @p dst with two transforms:
 *  - if the path starts with @c '\\' it is prefixed with @c D_800980CC
 *    (@c "x:\USPC\WORLD"): 14 bytes are copied (13 chars + NUL) and the write
 *    cursor advances 13, so the NUL is overwritten by the copied path;
 *  - a @c ';' (the start of an ISO @c ;1 version suffix) skips two source
 *    characters, dropping the suffix.
 *
 * The copy runs until (and including) the source NUL terminator. Example:
 * @c "\DAT\WMX.OBJ;1" becomes @c "x:\USPC\WORLD\DAT\WMX.OBJ".
 *
 * @param dst Destination buffer for the rewritten path.
 * @param src Source ISO9660 path string.
 */
void func_8009C54C(u8 *dst, u8 *src) {
    u8 ch;
    s32 i; /* source index */
    s32 j; /* destination index */

    i = 0;
    j = 0;
    for (;;) {
        if ((i == 0) && (src[i] == '\\')) {
            memcpy(&dst[j], D_800980CC, 14);
            j += 13;
        }
        if (src[i] == ';') {
            i += 2;
        }
        ch = src[i];
        dst[j++] = ch;
        if (ch == '\0') {
            break;
        }
        i++;
    }
}

extern RECT D_800C8640;

/**
 * @brief Pull a two-image descriptor from @p src and stream both TIMs
 *        through @c LoadImage, gated by bit 3 of the descriptor flag.
 *
 * Steps:
 *  1. @c func_8009CA34(@p src, @c &desc) — parse a streaming-image
 *     header into @c desc.
 *  2. Copy @c desc.rect1 into the scratch @c D_800C8640 and
 *     @c LoadImage(@c &D_800C8640, @c desc.data1).
 *  3. If bit 3 of @c desc.flag is set, copy @c desc.rect2 and
 *     @c LoadImage the second payload too.
 *
 * The scratch @c D_800C8640 is re-used between both loads — the GPU
 * blit is synchronous from this routine's POV, so the RECT can be
 * overwritten right after the call returns.
 *
 * @param src Source buffer fed to @c func_8009CA34.
 */
void func_8009C5FC(s32 *src) {
    ImageDesc desc;
    func_8009CA34(src, &desc);
    D_800C8640.x = desc.rect1.x;
    D_800C8640.y = desc.rect1.y;
    D_800C8640.w = desc.rect1.w;
    D_800C8640.h = desc.rect1.h;
    LoadImage(&D_800C8640, desc.data1);
    if ((desc.flag >> 3) & 1) {
        D_800C8640.x = desc.rect2.x;
        D_800C8640.y = desc.rect2.y;
        D_800C8640.w = desc.rect2.w;
        D_800C8640.h = desc.rect2.h;
        LoadImage(&D_800C8640, desc.data2);
    }
}

extern void sndSeqSetTempoAlt(s32 tempo);
extern void sndSetMasterVolume(s32 vol);
extern void sndCmdF1(void);

/**
 * @brief Silence the audio subsystem: zero tempo, zero master volume, send cmd F1.
 *
 * Likely used when leaving a map or suspending sound playback. The @c sndCmdF1
 * call appears in similar "stop audio" sequences elsewhere in the codebase.
 */
void func_8009C69C(void) {
    sndSeqSetTempoAlt(0);
    sndSetMasterVolume(0);
    sndCmdF1();
}

extern void sndSetChannelVolume(s32 channel, s32 vol);

/**
 * @brief Set a sound channel's volume, narrowing the volume arg to signed 8 bits.
 *
 * Forwards to sndSetChannelVolume with @p vol sign-extended from s8 to s32
 * (the canonical `sll 24 / sra 24` pair). Useful when the caller only has
 * an s8 volume value that needs to be promoted into the s32 SDK signature.
 *
 * @param channel Sound channel index.
 * @param vol Volume level treated as signed 8-bit.
 */
void func_8009C6CC(s32 channel, s8 vol) {
    sndSetChannelVolume(channel, vol);
}

extern SeqEntry D_800C4FD8[];
extern void sndSeqStartPan(s32 a0, s32 a1, s32 a2, s32 a3);

/**
 * @brief Start a sound sequence with pan for entry @p idx of the table.
 *
 * Looks up the sequence-table entry at @c D_800C4FD8[idx] and dispatches
 * @c sndSeqStartPan with its @c field10 and @c field04, plus the caller's
 * @p a1 and @p pan (sign-extended from s8).
 *
 * @param idx Index into the sequence table.
 * @param a1 Sound parameter forwarded as the third arg.
 * @param pan Signed 8-bit pan value (sign-extended to s32 for the ABI).
 */
void func_8009C6F0(s32 idx, s32 a1, s8 pan) {
    sndSeqStartPan(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04, a1, pan);
}

/**
 * @brief Stop the sound sequence associated with table entry @p idx.
 *
 * Dispatches @c sndCmd21(entry->field10, entry->field04) then clears the
 * entry's runtime-state halfword at +0x12.
 *
 * @param idx Index into the @c D_800C4FD8 sequence table.
 */
void func_8009C738(s32 idx) {
    sndCmd21(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04);
    D_800C4FD8[idx].field12 = 0;
}

extern void sndSeqPlayPan7bit(s32 a0, s32 a1, s32 a2, s32 a3);

/**
 * @brief Start a panned sound sequence with a 7-bit-clamped pan value.
 *
 * Clamps @p pan into [0, 0x7F] (7-bit range) before calling
 * @c sndSeqPlayPan7bit with the sequence-table entry's @c field10 and
 * @c field04, plus the forwarded @p arg.
 *
 * @param idx Index into the @c D_800C4FD8 sequence table.
 * @param arg Forwarded as the third arg of sndSeqPlayPan7bit.
 * @param pan Pan value; clamped to [0, 0x7F].
 */
void func_8009C780(s32 idx, s32 arg, s32 pan) {
    if (pan >= 0x80) pan = 0x7F;
    if (pan < 0) pan = 0;
    sndSeqPlayPan7bit(D_800C4FD8[idx].field10, D_800C4FD8[idx].field04, arg, pan);
}

/** Sets bit 0x20 on two related flag bytes. */
void func_8009C7DC(void) {
    D_800780D8[0x108] |= 0x20;
    D_800D23D8[0x66] |= 0x20;
}

/** Clears bit 0x20 on two related flag bytes. */
void func_8009C808(void) {
    D_800780D8[0x108] &= ~0x20;
    D_800D23D8[0x66] &= ~0x20;
}

extern s32 D_800C4D94;
extern void fadeOutSfxSlow(s32 idx);

/**
 * @brief Fade out SFX slot tracked by D_800C4D94 (if any) and clear the slot.
 *
 * If D_800C4D94 holds a non-negative value (an active slot handle), calls
 * @c fadeOutSfxSlow(1) to fade channel 1, then resets the tracker to -1
 * (inactive). A no-op when already inactive.
 */
void func_8009C834(void) {
    if (D_800C4D94 >= 0) {
        fadeOutSfxSlow(1);
        D_800C4D94 = -1;
    }
}

/** Advances index and returns difference from table lookup. */
u8 func_8009C870(void) {
    u8 idx;
    D_800C4FD6++;
    idx = D_800C4FD6;
    if (idx == 0) {
        D_800C4FD5 += 0xD;
    }
    return (D_800C4DCC[D_800C4FD6] - D_800C4FD5) & 0xFF;
}

/** Sets two related byte values. */
void func_8009C8CC(s32 val) {
    D_800C4FD6 = val;
    D_800C4FD5 = val;
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object1", func_8009C8E0);

/**
 * @brief Parse a streaming-image header out of @p src into @p desc,
 *        skipping any embedded data payload, and return the leading
 *        @c count word.
 *
 * Wire format of the buffer at @p src (u32 words unless noted):
 *  - @c +0x00 : count / magic word (returned, not interpreted here).
 *  - @c +0x04 : flag byte at @c [0] (only the low nibble is kept).
 *  - if bit 3 of the flag is set, an embedded payload follows:
 *    - @c +0x08 : payload size in bytes (rounded down to a multiple
 *      of 4, then the 12 leading header bytes are subtracted to get
 *      the data-stride to skip).
 *    - @c +0x0C : @c rect2 (x, y, w, h — four u16s).
 *    - @c +0x14 : @c data2 starts here; consumed (size - 12) bytes
 *      worth, after which @p src is advanced past it.
 *  - the next u32 boundary holds the @c rect1 header (x, y, w, h)
 *    followed by the @c data1 pointer.
 *
 * @param src   Source buffer.
 * @param desc  Output @ref ImageDesc — receives @c rect1, @c data1,
 *              optional @c rect2 / @c data2 (when the flag bit is
 *              set), and the @c flag itself.
 * @return      The leading @c count word at @p src[0]. Callers in
 *              this TU ignore it, but the value is real ABI.
 *
 * @note Matching needs the two-step @c size mask
 *       (@c size = (u32)size >> 2; @c size = size << 2;) instead of
 *       the one-line @c (((u32)size) >> 2) << 2 — gcc 2.8.0 otherwise
 *       picks a slightly different register cascade for the
 *       @c rect2.y / @c size temps.
 */
s32 func_8009CA34(s32 *src, ImageDesc *desc) {
    s32 count;
    s32 size;

    count = *src;
    src++;

    desc->flag = *(u8 *)src & 0xF;
    src++;

    if (desc->flag >> 3) {
        size = *src;
        src++;

        desc->rect2.x = ((u16 *)src)[0];
        desc->rect2.y = ((u16 *)src)[1];
        src++;

        size = (u32)size >> 2;
        size = size << 2;
        size -= 12;

        desc->rect2.w = ((u16 *)src)[0];
        desc->rect2.h = ((u16 *)src)[1];
        src++;

        desc->data2 = (u32 *)src;
        src = (s32 *)((u8 *)src + size);
    }

    src++;
    desc->rect1.x = ((u16 *)src)[0];
    desc->rect1.y = ((u16 *)src)[1];
    src++;

    desc->rect1.w = ((u16 *)src)[0];
    desc->rect1.h = ((u16 *)src)[1];
    src++;

    desc->data1 = (u32 *)src;

    return count;
}

/**
 * @brief 16-byte CD load list entry — NULL-terminated by @c marker = 0.
 *
 * @c marker holds an arbitrary non-zero value while the entry is live; the
 * walker stops on the first entry whose @c marker is 0.
 */
typedef struct {
    /* 0x00 */ s32 marker;
    /* 0x04 */ u8 *dest;
    /* 0x08 */ s32 lba;
    /* 0x0C */ u32 size;
} CdLoadEntry;

extern void func_80042634(s32 a);

/**
 * @brief Walk a NULL-terminated @ref CdLoadEntry list, issuing a @c cdRead
 *        for each entry and spinning on @c func_800393C8 between entries.
 *
 * For each non-terminator entry, kicks off @c cdRead(lba, size, dest, NULL)
 * then busy-waits in a poll loop on @c func_800393C8 until it returns 0
 * (read complete). While polling, optionally calls @p spin_cb each iteration
 * after a @c func_80042634(0) prep step — used by callers to keep the frame
 * advancing / GPU buffer flushing during the wait.
 *
 * Returns immediately if the first entry's marker is 0.
 *
 * @param entries  CD load list (NULL-terminated by @c marker = 0).
 * @param spin_cb  Optional polling callback (may be @c NULL).
 *
 * @note Uses a @c goto top loop to suppress gcc 2.8.0's induction-variable
 *       optimizer — without it, the @c entries pointer is split into a
 *       second @c entries+4 base register for the field cluster
 *       (@c lba/@c size/@c dest), forcing @p spin_cb out to @c s2 and
 *       adding 3 extra instructions. The byte-perfect output requires a
 *       single base register, which structured loops can't produce here.
 */
void func_8009CAE0(CdLoadEntry *entries, void (*spin_cb)(void)) {
    CdLoadEntry *e;
    if (entries->marker == 0) return;
    e = entries;
top:
    cdRead(e->lba, e->size, e->dest, 0);
    while (func_800393C8() != 0) {
        if (spin_cb != 0) {
            func_80042634(0);
            spin_cb();
        }
    }
    e++;
    if (e->marker != 0) goto top;
}

#define CD_SECTOR_SIZE 0x800 /* bytes per CD sector */
#define ID_LIST_END    0xFF  /* terminator id */
#define ID_LIST_MAX    2     /* id slots in the descriptor */
#define ID_OFFSET      2     /* id bytes begin 2 into the descriptor */

/** Section descriptor: provides the base sector that object ids index from. */
typedef struct {
    u8  pad0[8];
    s32 baseLba; /* sector number of id 0; cdRead reads baseLba + id */
} CdSection;

/**
 * @brief Load up to two CD sectors named by an object descriptor.
 *
 * Reads the id bytes at @c desc[ID_OFFSET] and @c desc[ID_OFFSET+1] (at most
 * @c ID_LIST_MAX, stopping at the @c ID_LIST_END terminator). For each id it
 * reads one @c CD_SECTOR_SIZE sector at @c section->baseLba @c + @c id into a
 * destination buffer — @p dest0 for the first id, @p dest1 for the rest — and
 * spins on @c func_800393C8 until that read finishes.
 *
 * @param desc    Object descriptor; the id bytes begin at offset @c ID_OFFSET.
 * @param section Provides the base sector the ids are added to.
 * @param dest0   Destination buffer for the first id's sector.
 * @param dest1   Destination buffer for any subsequent id's sector.
 */
void func_8009CB70(u8 *desc, CdSection *section, u8 *dest0, u8 *dest1) {
    u8 *p;
    u8 *first;
    u8 *end;
    u32 id;
    s32 term;

    if (desc[ID_OFFSET] == ID_LIST_END) {
        return;
    }
    /* hold the terminator in a local so it stays in a saved register across
       the cdRead / func_800393C8 calls rather than being reloaded each pass */
    term = ID_LIST_END;
    p = desc;
    end = p + ID_LIST_MAX;
    first = p;
loop:
    /* signed compare matches the original's slt (the ids sit just past the
       descriptor header, so the addresses never cross the sign boundary) */
    if ((s32)p >= (s32)end) {
        goto done;
    }
    id = p[ID_OFFSET];
    cdRead(section->baseLba + id, CD_SECTOR_SIZE, (p != first) ? dest1 : dest0, 0);
    while (func_800393C8()) {
        ;
    }
    p++;
    if (p[ID_OFFSET] != term) {
        goto loop;
    }
done:;
}

void func_8009CC34(void) {
}

/** @brief Advance D_800C4FD4 index, add 0xD to D_800C4FD3 on wraparound, return table diff. */
s32 func_8009CC3C(void) {
    D_800C4FD4++;
    if ((u8)D_800C4FD4 == 0) {
        D_800C4FD3 += 0xD;
    }
    return (u8)(D_800C4DCC[D_800C4FD4] - D_800C4FD3);
}

/** @brief Increment D_800C4FD7 index, return D_800C4DCC table value at new index. */
s32 func_8009CC98(void) {
    D_800C4FD7++;
    return D_800C4DCC[D_800C4FD7];
}

/** Sets two related byte values. */
void func_8009CCC8(s32 val) {
    D_800C4FD4 = val;
    D_800C4FD3 = val;
}

/** Sets a single byte value. */
void func_8009CCDC(s32 val) {
    D_800C4FD7 = val;
}
