#include "common.h"
#include "psxsdk/libgpu.h"
#include "world.h"

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A64DC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A688C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A6A74);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A6BE0);

/**
 * @brief Splice three of @c a0's bone prims into two pairs of OT slots.
 *
 * For each of the three bone ids in @c D_800C53B8[0], @c [3], @c [4],
 * link the entity's prim pointer between the slot-A and slot-B halves
 * of an OT pair, twice — once for the primary OT array at
 * @c D_800D3E98 and once for the secondary at @c D_800D40D8. The cond
 * bit picks the @c [0] or @c [1] row of the array, switching layout
 * for non-canonical entity models.
 *
 * @param a0 Entity model whose @c primList holds the prims to splice.
 */
void func_800A735C(EntityModel *a0) {
    s32 cond = (a0 != &D_800CA040) ? 1 : 0;

    addPrims(&a0->primList[D_800C53B8[0]], &D_800D3E50[cond][0].link, &D_800D3E98[cond][0].link);
    addPrims(&a0->primList[D_800C53B8[3]], &D_800D3E50[cond][1].link, &D_800D3E98[cond][1].link);
    addPrims(&a0->primList[D_800C53B8[4]], &D_800D3E50[cond][2].link, &D_800D3E98[cond][2].link);

    addPrims(&a0->primList[D_800C53B8[0]], &D_800D4090[cond][0].link, &D_800D40D8[cond][0].link);
    addPrims(&a0->primList[D_800C53B8[3]], &D_800D4090[cond][1].link, &D_800D40D8[cond][1].link);
    addPrims(&a0->primList[D_800C53B8[4]], &D_800D4090[cond][2].link, &D_800D40D8[cond][2].link);
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7590);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7B38);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7CD0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A7E74);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8024);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8270);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8400);

extern DR_MODE D_800D4FB0[2][96];

/**
 * @brief Initialise two 96-slot DR_MODE pools with fixed tag fields.
 *
 * For each of the two pools at @c D_800D4FB0[0..1], walks 96 DR_MODE
 * slots and sets @c len = 2 (2-word payload) and @c code = 0x6A via
 * the PsyQ @c setlen/@c setcode macros. Used to prime the primitive
 * templates before per-frame GPU command generation fills in the rest.
 */
void func_800A84D0(void) {
    s32 s, i;
    for (s = 0; s < 2; s++) {
        for (i = 0; i < 96; i++) {
            setlen(&D_800D4FB0[s][i], 2);
            setcode(&D_800D4FB0[s][i], 0x6A);
        }
    }
}

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8524);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8868);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8A28);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A8C1C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9254);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9300);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9CC0);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9E24);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9ED4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800A9F54);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AA210);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAD48);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAE04);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAEAC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAED4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAF84);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AAFBC);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB02C);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB06C);


INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB100);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB2D4);

INCLUDE_ASM("asm/ovl/world/nonmatchings/we_object4", func_800AB300);
