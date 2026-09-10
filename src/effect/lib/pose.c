/**
 * @file pose.c
 * @brief Walk a pose script and hand each posed part to a callback.
 */
#include "common.h"
#include "effect.h"
#include "effect/lib/fill.h"
#include "effect/lib/pose.h"

/**
 * @name Pose channel flags
 *
 * @ref EffectPosePart::flags carries one trio per channel: the channel is
 * keyed, it also carries a velocity, it also carries an acceleration. The
 * trios are the translation, the rotation, the scale and the colour, in the
 * order @ref effectPoseWalk walks them; the top two bits are single channels
 * of their own, the depth ramp and the vertex-animation cursor.
 * @{
 */
#define EFFECT_POSE_KEY0 0x1
#define EFFECT_POSE_VEL0 0x2
#define EFFECT_POSE_ACC0 0x4
#define EFFECT_POSE_KEY1 0x8
#define EFFECT_POSE_VEL1 0x10
#define EFFECT_POSE_ACC1 0x20
#define EFFECT_POSE_SCALE_KEY 0x40
#define EFFECT_POSE_SCALE_VEL 0x80
#define EFFECT_POSE_SCALE_ACC 0x100
#define EFFECT_POSE_KEY3 0x200
#define EFFECT_POSE_VEL3 0x400
#define EFFECT_POSE_ACC3 0x800
#define EFFECT_POSE_DEPTH 0x1000
#define EFFECT_POSE_ANIM 0x2000
/** @} */

/** @brief One animated part of a posed model; its flags size its pose slot. */
typedef struct {
    /* 0x00 */ s32 unk000;
    /* 0x04 */ s32 flags;
} EffectPosePart;

/** @brief The part list a posed model carries, each part at a byte offset. */
typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 unk004;
    /* 0x08 */ s32 offsets[1];
} EffectPoseParts;

/** @brief Ceiling of the depth and vertex-animation ramps: @ref ONE in 16.16. */
#define EFFECT_POSE_RAMP_MAX 0x10000000

static void effectPoseFindShorts(s32 key, s32 *table, s16 *out);
static void effectPoseFindWords(s32 key, s32 *table, s32 *out);
static s32 *effectPoseStepChannel(s32 flags, u8 *base, s32 **cursorPtr,
                                  s32 *slot, s32 key, s16 *out);

/**
 * @brief Clear @p size bytes of pose state at @p out and seed it from @p model.
 *
 * Every part of the model takes a slot sized by its flags; the three-word
 * scale slot starts at 1.0 rather than zero.
 */
void effectPoseInit(EffectPoseModel *model, s32 *out, s32 size) {
    EffectPoseParts *parts;
    EffectPosePart *part;
    s32 *offset;
    s32 *slot;
    s32 count;
    s32 flags;
    s32 i;

    effectFillWords(out, 0, size >> 2);
    out[0] = (s32)model;
    slot = out + 2;
    parts = (EffectPoseParts *)((u8 *)model + model->partsOffset);
    count = parts->count;
    offset = parts->offsets;
    for (i = 0; i < count; i++) {
        part = (EffectPosePart *)((u8 *)parts + *offset++);
        flags = part->flags;
        if (flags & EFFECT_POSE_ACC0) {
            slot += 9;
        } else if (flags & EFFECT_POSE_VEL0) {
            slot += 6;
        } else if (flags & EFFECT_POSE_KEY0) {
            slot += 3;
        }
        if (flags & EFFECT_POSE_ACC1) {
            slot += 9;
        } else if (flags & EFFECT_POSE_VEL1) {
            slot += 6;
        } else if (flags & EFFECT_POSE_KEY1) {
            slot += 3;
        }
        if (flags & (EFFECT_POSE_SCALE_KEY | EFFECT_POSE_SCALE_VEL |
                     EFFECT_POSE_SCALE_ACC)) {
            slot[2] = ONE << 16;
            slot[1] = ONE << 16;
            slot[0] = ONE << 16;
        }
        if (flags & EFFECT_POSE_SCALE_ACC) {
            slot += 9;
        } else if (flags & EFFECT_POSE_SCALE_VEL) {
            slot += 6;
        } else if (flags & EFFECT_POSE_SCALE_KEY) {
            slot += 3;
        }
        if (flags & EFFECT_POSE_ACC3) {
            slot += 6;
        } else if (flags & EFFECT_POSE_VEL3) {
            slot += 4;
        } else if (flags & EFFECT_POSE_KEY3) {
            slot += 2;
        }
        if (flags & EFFECT_POSE_DEPTH) {
            slot += 3;
        }
        if (flags & EFFECT_POSE_ANIM) {
            slot += 4;
        }
    }
}

/**
 * @brief Look @p key up in a table of 12-byte entries and copy its three
 *        halfwords out.
 *
 * The entries are ordered by key and closed by -1; nothing is written when the
 * key is missing.
 */
static void effectPoseFindShorts(s32 key, s32 *table, s16 *out) {
    s32 k = *table;

    while (k < key) {
        if (-1 == k) {
            return;
        }
        table += 3;
        k = *table;
    }
    if (k == key) {
        table++;
        *out++ = ((s16 *)table)[0];
        *out++ = ((s16 *)table)[1];
        *out = ((s16 *)table)[2];
    }
}

/**
 * @brief As @ref effectPoseFindShorts, for 16-byte entries holding three words.
 */
static void effectPoseFindWords(s32 key, s32 *table, s32 *out) {
    s32 k = *table;

    while (k < key) {
        if (-1 == k) {
            return;
        }
        table += 4;
        k = *table;
    }
    if (k == key) {
        table++;
        *out++ = *table++;
        *out = table[0];
        out[1] = table[1];
    }
}

/**
 * @brief Step one channel of a pose and hand its current value to @p out.
 *
 * @param flags  Bit 0 keyed, bit 1 velocity, bit 2 acceleration.
 * @param base   The keyed tables are at byte offsets from here.
 * @param cursorPtr Walks the channel's table offsets; advanced past the ones read.
 * @param slot   The channel's state; position, then velocity, then acceleration.
 * @param key    Frame the keyed tables are looked up at.
 * @param out    Receives the channel's three components.
 * @return The state past this channel.
 *
 * @note The gotos are the shape the original had: each arm falls into the next
 *       one's body, so the move and emit blocks exist once. Written as an
 *       if/else-if chain the compiler copies them instead (143 instructions
 *       against 124); written as three independent tests it reaches the emit
 *       block through a mask of 6 and 7 rather than 2 and 1.
 */
static s32 *effectPoseStepChannel(s32 flags, u8 *base, s32 **cursorPtr,
                                  s32 *slot, s32 key, s16 *out) {
    s32 *cursor = *cursorPtr;
    s32 *vel;
    s32 *acc;
    s16 *p;
    s32 v;

    if (flags & EFFECT_POSE_ACC0) {
        vel = slot + 3;
        acc = slot + 6;
        vel[0] += acc[0];
        vel[1] += acc[1];
        vel[2] += acc[2];
        goto move;
    }
    if (flags & EFFECT_POSE_VEL0) {
        vel = slot + 3;
move:
        slot[0] += vel[0];
        slot[1] += vel[1];
        slot[2] += vel[2];
        p = (s16 *)slot;
        goto emit;
    }
    if (flags & EFFECT_POSE_KEY0) {
        p = (s16 *)slot;
emit:
        v = p[1];
        out[0] = v;
        v = p[3];
        out[1] = v;
        v = p[5];
        out[2] = v;
    }
    if (flags & EFFECT_POSE_KEY0) {
        effectPoseFindWords(key, (s32 *)(base + *cursor++), slot);
        v = ((s16 *)slot)[1];
        slot++;
        out[0] = v;
        v = ((s16 *)slot)[1];
        slot++;
        out[1] = v;
        v = ((s16 *)slot)[1];
        slot++;
        out[2] = v;
    } else if (flags & (EFFECT_POSE_VEL0 | EFFECT_POSE_ACC0)) {
        slot += 3;
    }
    if (flags & EFFECT_POSE_VEL0) {
        effectPoseFindWords(key, (s32 *)(base + *cursor++), slot);
        slot += 3;
    } else if (flags & EFFECT_POSE_ACC0) {
        slot += 3;
    }
    if (flags & EFFECT_POSE_ACC0) {
        effectPoseFindWords(key, (s32 *)(base + *cursor++), slot);
        slot += 3;
    }
    *cursorPtr = cursor;
    return slot;
}

/**
 * @brief Walk one pose script and hand every part it poses to @p fn.
 *
 * The script opens with a part count and a frame count, then one byte offset
 * per part. A part entry names the model part it drives and the channels it
 * carries, and is followed by one keyed table per channel the flags claim.
 * @p pose holds the model, the frame the script is on, and then the running
 * state of every channel of every part -- the layout @ref effectPoseInit sizes.
 * That state is one array of words, but a channel holds either words or pairs
 * of halfwords, which is what every cast of @c slot below is reading.
 *
 * A channel that carries an acceleration steps its velocity by it, and one
 * that carries a velocity steps its value; the keyed table is read after that,
 * and overwrites the state only on a frame it holds. Channels the flags leave
 * out keep whatever the zeroed pose gives them, scale excepted -- that starts
 * at 1.0.
 *
 * @param pose The model and the frame the script is on.
 * @param fn   Called once per part, with the pose the script left.
 * @param req  Draw state handed straight to @p fn.
 * @return The frames left after this one; 0 once the script has run out.
 *
 * @note The gotos are the shape the original had. The colour channel's three
 *       arms fall into each other -- acceleration into the velocity ramp, and
 *       that into the keyed arm's colour store -- so each body exists once;
 *       spelled as an if/else-if chain the compiler copies them instead, the
 *       same way it does in @ref effectPoseStepChannel. The two out of the
 *       table walks land on the code that keeps the stepped value: reaching it
 *       with a @c break costs an instruction, because the frame comparison the
 *       walk falls out to is then on the path.
 */
s32 effectPoseWalk(s32 *pose, EffectPartFn fn, EffectDrawRequest *req) {
    EffectPartPose part;
    s32 *cursor;
    s32 *script;
    s32 *offsets;
    s32 *depthTab;
    u8 *entry;
    s32 *slot;
    u16 *vel;
    u16 *rgb;
    u16 frame;
    u16 *acc;
    s32 count;
    s32 frames;
    u32 flags;
    s32 depthKey;
    s32 i;
    s32 v;
    s32 *animTab;
    s32 animKey;
    s32 value;
    s32 limit;

    script = (s32 *)((u8 *)pose[0] + ((s32 *)pose[0])[1]);
    frames = script[1];
    if (pose[1] >= frames) {
        return 0;
    }
    offsets = script + 2;
    slot = pose + 2;
    count = script[0];
    for (i = 0; i < count; i++) {
        effectFillWords((s32 *)&part, 0, sizeof(part) / sizeof(s32));
        entry = (u8 *)script + *offsets++;
        part.index = i;
        part.scale[2] = ONE;
        part.scale[1] = ONE;
        part.scale[0] = ONE;
        cursor = (s32 *)entry;
        part.unk004 = *cursor++;
        part.mesh = (u8)part.unk004;
        flags = *cursor++;
        slot = effectPoseStepChannel(flags, entry, &cursor, slot, pose[1],
                                     part.pos);
        slot = effectPoseStepChannel(flags >> 3, entry, &cursor, slot, pose[1],
                                     part.rot);
        slot = effectPoseStepChannel(flags >> 6, entry, &cursor, slot, pose[1],
                                     part.scale);
        if (flags & EFFECT_POSE_ACC3) {
            vel = (u16 *)slot + 4;
            acc = (u16 *)slot + 8;
            vel[0] += acc[0];
            vel[1] += acc[1];
            vel[2] += acc[2];
            goto ramp;
        }
        if (flags & EFFECT_POSE_VEL3) {
            vel = (u16 *)slot + 4;
ramp:
            rgb = (u16 *)slot;
            v = rgb[0] + (s16)vel[0];
            if (v < 0) {
                v = 0;
            } else if (v > 0xFFFF) {
                v = 0xFFFF;
            }
            rgb[0] = v;
            v = rgb[1] + (s16)vel[1];
            if (v < 0) {
                v = 0;
            } else if (v > 0xFFFF) {
                v = 0xFFFF;
            }
            rgb[1] = v;
            v = rgb[2] + (s16)vel[2];
            if (v < 0) {
                v = 0;
            } else if (v > 0xFFFF) {
                v = 0xFFFF;
            }
            rgb[2] = v;
            goto emit;
        }
        if (flags & EFFECT_POSE_KEY3) {
            rgb = (u16 *)slot;
emit:
            part.colour.r = rgb[0] >> 8;
            part.colour.g = rgb[1] >> 8;
            part.colour.b = rgb[2] >> 8;
            part.colour.cd = 0;
        }
        if (flags & EFFECT_POSE_KEY3) {
            effectPoseFindShorts(pose[1], (s32 *)(entry + *cursor++),
                                 (s16 *)slot);
            part.colour.r = *(u16 *)slot >> 8;
            slot = (s32 *)((u16 *)slot + 1);
            part.colour.g = *(u16 *)slot >> 8;
            slot = (s32 *)((u16 *)slot + 1);
            part.colour.b = *(u16 *)slot >> 8;
            part.colour.cd = 0;
            slot++;
        } else if (flags & (EFFECT_POSE_VEL3 | EFFECT_POSE_ACC3)) {
            slot += 2;
        }
        if (flags & EFFECT_POSE_VEL3) {
            effectPoseFindShorts(pose[1], (s32 *)(entry + *cursor++),
                                 (s16 *)slot);
            slot += 2;
        } else if (flags & EFFECT_POSE_ACC3) {
            slot += 2;
        }
        if (flags & EFFECT_POSE_ACC3) {
            effectPoseFindShorts(pose[1], (s32 *)(entry + *cursor++),
                                 (s16 *)slot);
            slot += 2;
        }
        if (flags & EFFECT_POSE_DEPTH) {
            slot[1] += slot[2];
            slot[0] += slot[1];
            if (slot[0] < 0) {
                slot[0] = 0;
            } else {
                limit = EFFECT_POSE_RAMP_MAX;
                if (slot[0] > limit) {
                    slot[0] = limit;
                }
            }
            depthTab = (s32 *)(entry + *cursor++);
            depthKey = *depthTab;
            while (depthKey < pose[1]) {
                if (depthKey == -1) {
                    goto noDepthKey;
                }
                depthTab += 4;
                depthKey = *depthTab;
            }
            if (depthKey == pose[1]) {
                depthTab++;
                value = *depthTab++;
                *slot++ = value;
                part.depth = value >> 16;
                *slot++ = *depthTab;
                *slot++ = depthTab[1];
            } else {
noDepthKey:
                value = ((s16 *)slot)[1];
                part.depth = value;
                slot += 3;
            }
        }
        if (flags & EFFECT_POSE_ANIM) {
            slot[2] += slot[3];
            slot[1] += slot[2];
            if (slot[1] < 0) {
                slot[1] = 0;
            } else {
                limit = EFFECT_POSE_RAMP_MAX;
                if (slot[1] > limit) {
                    slot[1] = limit;
                }
            }
            animTab = (s32 *)(entry + *cursor++);
            animKey = *animTab;
            while (animKey < pose[1]) {
                if (animKey == -1) {
                    goto noAnimKey;
                }
                animTab += 5;
                animKey = *animTab;
            }
            if (animKey == pose[1]) {
                animTab++;
                frame = *(u16 *)animTab;
                *(u16 *)slot = frame;
                part.frameA = frame;
                slot = (s32 *)((u16 *)slot + 1);
                animTab = (s32 *)((u16 *)animTab + 1);
                frame = *(u16 *)animTab;
                *(u16 *)slot = frame;
                part.frameB = frame;
                slot = (s32 *)((u16 *)slot + 1);
                animTab = (s32 *)((u16 *)animTab + 1);
                value = *animTab++;
                *slot++ = value;
                part.weight = value >> 16;
                *slot++ = *animTab;
                *slot++ = animTab[1];
            } else {
noAnimKey:
                part.frameA = *(u16 *)slot;
                slot = (s32 *)((u16 *)slot + 1);
                part.frameB = *(u16 *)slot;
                slot = (s32 *)((u16 *)slot + 1);
                value = ((s16 *)slot)[1];
                part.weight = value;
                slot += 3;
            }
        }
        fn(pose, &part, req);
    }
    pose[1]++;
    return frames - pose[1];
}
