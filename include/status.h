#ifndef STATUS_H
#define STATUS_H

// Persistent statuses that continue to apply after battle
enum PersistentStatusFlag {
    STATUS_KO = 0x01, // knocked out
    STATUS_POISON = 0x02,
    STATUS_PETRIFY = 0x04,
    STATUS_DARKNESS = 0x08, // blind
    STATUS_SILENCE = 0x10,
    STATUS_BERSERK = 0x20,
    STATUS_ZOMBIE = 0x40,
    STATUS_HP_CRITICAL = 0x100, // HP below a quarter of max
    STATUS_HP_LOW = 0x200 // HP below half of max
};

// Statuses that only apply during battles and disappear after battle
enum BattleStatusFlag {
    BATTLE_STATUS_SLEEP = 0x01,
    BATTLE_STATUS_HASTE = 0x02,
    BATTLE_STATUS_SLOW = 0x04,
    BATTLE_STATUS_STOP = 0x08,
    BATTLE_STATUS_PROTECT = 0x20,
    BATTLE_STATUS_SHELL = 0x40,
    BATTLE_STATUS_REFLECT = 0x80,
    BATTLE_STATUS_CURSE = 0x200,
    BATTLE_STATUS_DOUBLE = 0x20000,
    BATTLE_STATUS_TRIPLE = 0x40000
};

#endif /* STATUS_H */
