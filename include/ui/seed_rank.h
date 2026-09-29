#ifndef UI_SEED_RANK_H
#define UI_SEED_RANK_H

#include "common.h"

/** @brief @c SeedRankNotification.state values, in the order the notification goes through them. */
typedef enum {
    SEED_RANK_NOTIFICATION_SHOW = 0, /**< Starts over from @c roll and @c slide 0. */
    SEED_RANK_NOTIFICATION_SLIDING_IN = 1,
    SEED_RANK_NOTIFICATION_HOLD = 2,
    SEED_RANK_NOTIFICATION_HOLDING = 3,
    SEED_RANK_NOTIFICATION_ROLL = 4, /**< Rolls only when the rank changed. */
    SEED_RANK_NOTIFICATION_ROLLING = 5,
    SEED_RANK_NOTIFICATION_SHOWN = 6,
    SEED_RANK_NOTIFICATION_HIDE = 7,
    SEED_RANK_NOTIFICATION_SLIDING_OUT = 8,
    SEED_RANK_NOTIFICATION_HIDDEN = 9,
    SEED_RANK_NOTIFICATION_STATE_10 = 10 /**< Treated as hidden; nothing sets it. */
} SeedRankNotificationState;

/**
 * @brief The SeeD rank notification: it slides in, rolls the rank and the salary
 * from their old values to the new ones, and slides out again.
 */
typedef struct {
    u16 state; /* 0x00: a SeedRankNotificationState */
    u8 salaryEnabled; /* 0x02: nonzero while the SeeD salary is on */
    u8 pad03; /* 0x03 */
    s16 roll; /* 0x04: 0 shows the old rank and salary, ONE the new ones */
    s16 slide; /* 0x06: 0 is off screen and not drawn, ONE is fully in */
    u8 oldRank; /* 0x08 */
    u8 newRank; /* 0x09 */
    u8 timer; /* 0x0A */
    u8 salaryBlanks; /* 0x0B: leading blanks the two salary strings share */
    u8 newRankText[3]; /* 0x0C */
    u8 oldRankText[3]; /* 0x0F */
    u8 newSalaryText[6]; /* 0x12 */
    u8 oldSalaryText[6]; /* 0x18 */
} SeedRankNotification;

extern void updateSeedRankNotification(void);
extern u8 *drawSeedRankNotification(void *ot, u8 *pkt);
extern void hideSeedRankNotification(void);
extern void showSeedRankNotification(s32 oldRank, s32 newRank, s32 oldSalary, s32 newSalary);
extern void setSalaryEnabled(s32 enabled);
extern void resetSeedRankNotification(void);

#endif
