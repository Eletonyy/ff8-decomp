#include "common.h"
#include "psxsdk/libgpu.h"
#include "psxsdk/libgte.h"
#include "battle_anim.h"
#include "btl_anim.h"
#include "numstr.h"
#include "ui/icon.h"
#include "ui/seed_rank.h"

/** @brief Primitive code 0x40 (flat line) in the colour word. */
#define LINE_F2_CODE 0x40000000

/* --- Externs (sorted by address) --- */

extern u8 rankAText[];

/* --- Private functions --- */

static u8 *drawHudText(P_TAG *ot, u8 *pkt, u8 *str, s32 x, s32 y, s32 color);
static void *drawSeedRankNotificationUnderlines(P_TAG *ot, LINE_F2 *line, s32 leftWidth, s32 rightX);

/**
 * @brief Step the SeeD rank notification by one frame.
 *
 * It slides in, holds for 24 frames, rolls to the new rank and stays shown
 * until hideSeedRankNotification, then slides out. @c slide and @c roll run from 0 to ONE.
 */
void updateSeedRankNotification(void) {
    u16 *state = &g_engine.seedRankNotification.state;
    SeedRankNotification *p = &g_engine.seedRankNotification;

    switch (*state) {
    case SEED_RANK_NOTIFICATION_SHOW:
        p->roll = 0;
        p->slide = 0;
        *state = SEED_RANK_NOTIFICATION_SLIDING_IN;
    case SEED_RANK_NOTIFICATION_SLIDING_IN:
        p->slide += ONE / 16;
        if (p->slide < ONE) {
            return;
        }
        p->slide = ONE;
        *state = SEED_RANK_NOTIFICATION_HOLD;
        return;
    case SEED_RANK_NOTIFICATION_HOLD:
        p->timer = 24;
        *state = SEED_RANK_NOTIFICATION_HOLDING;
    case SEED_RANK_NOTIFICATION_HOLDING:
        p->timer--;
        if (p->timer) {
            return;
        }
        *state = SEED_RANK_NOTIFICATION_ROLL;
        return;
    case SEED_RANK_NOTIFICATION_ROLL:
        if (p->newRank == p->oldRank) {
            p->roll = ONE;
            *state = SEED_RANK_NOTIFICATION_SHOWN;
            return;
        }
        *state = SEED_RANK_NOTIFICATION_ROLLING;
        return;
    case SEED_RANK_NOTIFICATION_ROLLING:
        p->roll += ONE / 12;
        if (p->roll < ONE) {
            return;
        }
        p->roll = ONE;
        *state = SEED_RANK_NOTIFICATION_SHOWN;
        return;
    case SEED_RANK_NOTIFICATION_SHOWN:
        return;
    case SEED_RANK_NOTIFICATION_HIDE:
        *state = SEED_RANK_NOTIFICATION_SLIDING_OUT;
        return;
    case SEED_RANK_NOTIFICATION_SLIDING_OUT:
        p->slide -= ONE / 16;
        if (p->slide > 0) {
            return;
        }
        p->slide = 0;
        *state = SEED_RANK_NOTIFICATION_HIDDEN;
        return;
    case SEED_RANK_NOTIFICATION_HIDDEN:
    case SEED_RANK_NOTIFICATION_STATE_10:
        return;
    }
}


/**
 * @brief Draw a zero-terminated glyph string, 9 pixels per character.
 *
 * Character 7 is a blank: it draws nothing but still takes its 9 pixels.
 * Every other character is drawn with drawIcon.
 *
 * @param ot OT base pointer.
 * @param pkt Current packet buffer pointer.
 * @param str Zero-terminated glyph string.
 * @param x Left edge of the first character.
 * @param y Top edge.
 * @param color Color word passed to drawIcon.
 * @return Updated packet buffer pointer after rendering.
 */
static u8 *drawHudText(P_TAG *ot, u8 *pkt, u8 *str, s32 x, s32 y, s32 color) {
    P_TAG *otBase = ot;
    u8 *ptr = str;
    s32 xPos = x;
    s32 yPos = y;
    s32 blank = 7;
    u8 ch;
    s32 col = color;

    do {
    top:
        ch = *ptr++;
        if (ch == 0) {
            return pkt;
        }
        if (ch == blank) goto skip;
        /* Emit one glyph packet and advance the cursor. */
        pkt = drawIcon(otBase, pkt, ch, xPos, yPos, col);
    } while (0);
skip:
    xPos = xPos + 9;
    goto top;
}


/**
 * @brief Draw two pairs of horizontal lines: y=209 in a bright grey and y=210 in a dim
 * grey, from x=0 to @p leftWidth + 50 and from @p rightX to 320.
 *
 * The greys are the HUD brightness / 32 and / 128.
 *
 * @param ot Ordering table.
 * @param line Where to build the four LINE_F2 packets.
 * @param leftWidth The left lines end at @p leftWidth + 50.
 * @param rightX Where the right lines start.
 * @return The packet cursor after the lines.
 */
static void *drawSeedRankNotificationUnderlines(P_TAG *ot, LINE_F2 *line, s32 leftWidth, s32 rightX) {
    u32 bright;
    u32 dim;
    s32 x; /* Regalloc: the intensity, then the left lines' x0. That later use keeps the / 128 rounding in a register of its own. */
    s32 c;
    u32 link1; /* Regalloc: one link temp per prim, each live from entry, gives the retail $t5..$t8. */
    u32 link2;
    u32 link3;
    u32 link4;

    x = g_engine.countdown.brightness;
    c = x / 32;
    bright = LINE_F2_CODE | (c << 16) | (c << 8) | c;
    c = x / 128;
    dim = LINE_F2_CODE | (c << 16) | (c << 8) | c;

    x = 0;
    setLineF2(line);
    *(u32 *)&line->r0 = bright; /* r, g, b and code in one store */
    line->x0 = x;
    line->x1 = leftWidth + 50;
    line->y0 = 209;
    line->y1 = 209;
    addPrimFastWithTempOperand(ot, line, link1);
    line++;
    setLineF2(line);
    *(u32 *)&line->r0 = dim;
    line->x0 = x;
    line->x1 = leftWidth + 50;
    line->y0 = 210;
    line->y1 = 210;
    addPrimFastWithTempOperand(ot, line, link2);
    line++;
    setLineF2(line);
    *(u32 *)&line->r0 = bright;
    line->x0 = rightX;
    line->x1 = 320;
    line->y0 = 209;
    line->y1 = 209;
    addPrimFastWithTempOperand(ot, line, link3);
    line++;
    setLineF2(line);
    *(u32 *)&line->r0 = dim;
    line->x0 = rightX;
    line->x1 = 320;
    line->y0 = 210;
    line->y1 = 210;
    addPrimFastWithTempOperand(ot, line, link4);
    line++;
    return line;
}


/**
 * @brief Draw the SeeD rank notification and link it into the OT.
 *
 * Draws nothing while @c slide is 0. The rank half (@c ICON_SEED_LEVEL, then the rank)
 * slides in from 50 pixels to the left and the salary half (the salary, then
 * @c ICON_SEED_GIL) from 59 pixels to the right, along g_animCurveFadeOut. Everything
 * is clipped to a 12-pixel strip at y 196, where the old and new values roll
 * vertically while @c roll is between 0 and ONE;
 * drawSeedRankNotificationUnderlines then underlines the notification.
 * The grey level is the HUD brightness. Reading it through g_engine, like
 * the notification, lets gcc reach it at -0x280 from the notification's base register, as
 * the retail code does.
 *
 * @param ot OT base pointer.
 * @param pkt Current packet buffer pointer.
 * @return Updated packet buffer pointer after rendering.
 */
u8 *drawSeedRankNotification(void *ot, u8 *pkt) {
    RECT rect;
    u8 *out;
    SeedRankNotification *p;
    u32 color;
    s32 y;
    s32 curve;
    s32 roll;
    s32 absDir;
    s32 x;
    s32 leftShiftFixed;
    s32 rightShift;
    s32 rankLabelX;
    s32 yOff;
    s32 link;
    u8 *next;
    u8 *pktAfterLines;
    s32 leftShift;

    out = pkt;
    p = &g_engine.seedRankNotification;

    if (p->slide <= 0) {
        return out;
    }

    copyDisplayRect(&rect);

    color = g_engine.countdown.brightness;
    color >>= 5;
    color &= 0xFF;
    color = (color | (color << 8)) | (color << 16);
    color |= SPRT_CODE;

    curve = ONE - p->slide;
    curve = g_animCurveFadeOut[curve / 64];
    roll = p->roll;
    curve <<= 6;

    if (p->oldRank < p->newRank) {
        absDir = -roll;
        yOff = 12;
    } else {
        absDir = roll;
        yOff = -12;
    }

    leftShiftFixed = -(curve * 50);
    y = 0xC4;
    leftShift = leftShiftFixed / ONE;
    rankLabelX = leftShift + 0x10;

    out = drawIcon(ot, out, ICON_SEED_LEVEL, rankLabelX, y, color);
    x = leftShift + 0x30;

    if (roll == 0) {
        out = drawHudText(ot, out, p->oldRankText, x, y, color);
    } else if (roll == ONE) {
        out = drawHudText(ot, out, p->newRankText, x, y, color);
    } else {
        y = absDir * 12 / ONE + 0xC4;
        next = drawHudText(ot, out, p->oldRankText, x, y, color);
        out = drawHudText(ot, next, p->newRankText, x, y + yOff, color);
    }

    y = 0xC4;
    rightShift = curve * 59 / ONE;
    x = rightShift + 0xF0;

    out = drawIcon(ot, out, ICON_SEED_GIL, rightShift + 0x11D, y, color);

    if (roll == 0) {
        out = drawHudText(ot, out, p->oldSalaryText, x, y, color);
    } else if (roll == ONE) {
        out = drawHudText(ot, out, p->newSalaryText, x, y, color);
    } else {
        y = absDir * 12 / ONE + 0xC4;
        next = drawHudText(ot, out, p->oldSalaryText, x, y, color);
        out = drawHudText(ot, next, p->newSalaryText, x, y + yOff, color);
    }

    rect.h = 12;
    rect.y += 0xC4;

    SetDrawArea((DR_AREA *)out, &rect);
    addPrimFastWithTempOperand(ot, out, link);

    pktAfterLines = drawSeedRankNotificationUnderlines(ot, (LINE_F2 *)((DR_AREA *)out + 1),
        rankLabelX, x + p->salaryBlanks * 9);

    return emitDrawEnvPackets(ot, pktAfterLines);
}


/** @brief Start sliding the SeeD rank notification out. */
void hideSeedRankNotification(void) {
    g_engine.seedRankNotification.state = SEED_RANK_NOTIFICATION_HIDE;
}


/**
 * @brief Show the SeeD rank notification for a rank change.
 *
 * Restarts the notification with @c roll and @c slide at 0 and runs its first step.
 * The ranks are written as two digits, or from 31 up as the rankAText string
 * (@c ICON_BLANK and @c ICON_SEED_RANK_A), and the salaries as five digits. Leading
 * zeros become blanks (7), and @c salaryBlanks gets the number of leading
 * blanks the two salaries share.
 *
 * @param oldRank Rank before the change.
 * @param newRank Rank after the change.
 * @param oldSalary Salary before the change.
 * @param newSalary Salary after the change.
 */
void showSeedRankNotification(s32 oldRank, s32 newRank, s32 oldSalary, s32 newSalary)
{
    u8 buf[16];
    SeedRankNotification *d;
    u8 *salaryDigits;
    s32 blanks;
    s32 blank;
    s32 i;

    d = &g_engine.seedRankNotification;
    d->state = SEED_RANK_NOTIFICATION_SHOW;
    d->roll = 0;
    d->slide = 0;
    d->timer = 0;
    d->oldRank = oldRank;
    d->newRank = newRank;

    salaryDigits = &buf[5];

    intToDecString(oldSalary, buf, ICON_THIN_DIGIT_0);
    copyString(d->oldSalaryText, salaryDigits);
    replaceLeadingZeros(d->oldSalaryText, 4, ICON_THIN_DIGIT_0, ICON_BLANK);

    intToDecString(newSalary, buf, ICON_THIN_DIGIT_0);
    copyString(d->newSalaryText, salaryDigits);
    replaceLeadingZeros(d->newSalaryText, 4, ICON_THIN_DIGIT_0, ICON_BLANK);

    if (oldRank < 0x1F)
    {
        intToDecStringShort(oldRank, buf, ICON_THIN_DIGIT_0);
        copyString(d->oldRankText, &buf[3]);
        replaceLeadingZeros(d->oldRankText, 1, ICON_THIN_DIGIT_0, ICON_BLANK);
    }
    else
    {
        copyString(d->oldRankText, rankAText);
    }

    if (newRank < 0x1F)
    {
        intToDecStringShort(newRank, buf, ICON_THIN_DIGIT_0);
        copyString(d->newRankText, &buf[3]);
        replaceLeadingZeros(d->newRankText, 1, ICON_THIN_DIGIT_0, ICON_BLANK);
    }
    else
    {
        copyString(d->newRankText, rankAText);
    }

    i = 0;
    blanks = 5;
    blank = 7;

    for (; i < 5; i++)
    {
        if (d->newSalaryText[i] != blank && i < blanks)
        {
            blanks = i;
            break;
        }
    }

    i = 0;
    blank = 7;

    for (; i < 5; i++)
    {
        if (d->oldSalaryText[i] != blank && i < blanks)
        {
            blanks = i;
            break;
        }
    }

    d->salaryBlanks = blanks;
    updateSeedRankNotification();
}


/** @brief Set whether the SeeD salary is enabled. */
void setSalaryEnabled(s32 enabled) {
    g_engine.seedRankNotification.salaryEnabled = enabled;
}


/**
 * @brief Hide the SeeD rank notification at once.
 *
 * Sets @ref SEED_RANK_NOTIFICATION_HIDDEN and clears @c roll, @c slide and both ranks.
 */
void resetSeedRankNotification(void) {
    SeedRankNotification *p = &g_engine.seedRankNotification;

    p->state = SEED_RANK_NOTIFICATION_HIDDEN;
    p->roll = 0;
    p->slide = 0;
    p->oldRank = 0;
    p->newRank = 0;
}
