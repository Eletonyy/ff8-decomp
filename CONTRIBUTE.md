# Contribution guidelines

This is a living and work-in-progress document so changes may and are likely to happen. But below are som general guidelines for things to consider when implementing code in this

* All .c files has a header file with the same name. E.g. src/battle/bc_object1.c has a corresponding include/battle/bc_object1.h.
  * The header file only holds the public interface of the unit; public typedefs/structs owned by the unit, and public prototypes.
  * Private functions (functions only called from within the unit itself) are not declared in the header. They are declared `static` in a prototype block at the top of the .c file, and defined `static` further down. Private typedefs/structs used only by that unit live at the top of the .c file as well.
    * Reason: a symbol should have exactly one declaration, and a private symbol has no business being visible to other translation units. Keeping it `static` and file-local makes the unit boundary explicit and stops other files from growing a dependency on internals.

Example header:
```
// main.h
#ifndef MAIN_H
#def MAIN_H

// Public typedefs/structs
typedef struct {
    s32 value0;
    s16 value1;
} SomeMainStruct;

// Public prototypes
SomeMainStruct *doSomething(void);

#endif
```

Example implementation file:
```
// main.c
#include "main.h"

// Private typedefs/structs
typedef struct {
    s32 value0;
} PrivateStruct;

// Private prototypes
static void doSomethingInternal(PrivateStruct *arg0);

SomeMainStruct *doSomething(void) {
    ...
}

static void doSomethingInternal(PrivateStruct *arg0) {
    ...
}
```

For models that don't have a clear "owner", there should be an overarching header file for the general area for what it is used. E.g. include/tripletriad/tripletriad.h.

The project has technical debt where symbols are defined multiple times and sometimes as different types. So the general rule is that no symbol should be declared more than once. Same goes for function prototypes. So any function that relies on some global symbol should get access through it via #include from the symbol owners header file. The debt isn't limited to symbols: older code also breaks other rules in this guide, e.g. with raw offsets. If you see debt, fix it. The fixed code must still match, so verify with `make full`.

* Note that overlays can reuse names, and that doesn't break the "declared once" rule.
  * Besides the main executable, the game has overlays: chunks of code, like the world map or battles, that are loaded into the same memory area when needed. Only one is loaded at a time, so two overlays can have completely different functions at the same address.
  * A function nobody has named yet is named after its address. So two unrelated functions in two overlays can get the same name, e.g. `func_8009A638` exists in both the world and the battle overlay.
  * These are two different functions, not one function declared twice. Don't merge them. Declare each one once, in a header that only its own overlay includes. The "declared once" rule is per binary: once in the main executable, or once in an overlay.
  * If the only header it could go in is also included by other overlays, declare it at the top of the .c file that uses it instead, with a comment saying why (see `src/menu/menusts/menusts.c`).
  * Once someone works out what the function does and gives it a real name, the clash goes away.

Example: `func_8009A000` is one function in the world overlay and a different one in the battle overlay.

* Project only has 100% matching functions, we don't do the #ifdef NONMATCHING pattern.

* Code should be portable. Meaning, no magic offsets or addresses that would make the game crash if a struct were to change or a symbol were to move.

Example: the game keeps a player struct at address 0x800E1000.
```
typedef struct {
    s32 hp;    // 0x0
    s32 mp;    // 0x4
    s32 level; // 0x8
} Player;

extern Player g_player; // at 0x800E1000
```

Bad:
```
void setLevel(s32 level) {
    *(s32 *)((u8 *)&g_player + 0x8) = level; // BAD! Wrong as soon as a field is added before level
    *(s32 *)0x800E1008 = level;              // BAD! Wrong as soon as g_player moves
}
```

Good:
```
void setLevel(s32 level) {
    g_player.level = level; // GOOD! The compiler works out the offset and the linker the address
}
```

If there is no struct for the data yet, define one, or check whether an existing struct fits (same size, same usage). Fields you don't understand yet get placeholder names that keep the offset visible, e.g. `unk8`.

A raw offset or a hard-coded address is never acceptable, not even when it's the only way you've found to make a function match. Unless the original dev actually would've used a hard-coded address, keep looking for a version that uses a struct, or leave the function as `INCLUDE_ASM` for now.

* Define flags where bitwise flag comparisons are made. Use placeholder names if their representation is unknown. Either as #defines or enums. Use one value per bit, i.e. no multi-bit constants.

* No unnecessary casts, fix the symbol type instead. Give the local, the struct field or the prototype the type the code actually wants.
** A cast that survives review has to earn it. Either it changes the generated code and a comment says so, or it is a genuine reinterpretation of memory that no single type can express (a packed word copied across a pair of s16 fields, one buffer doing double duty on a path where its first use is dead).
** Casting at a call site to silence a type mismatch is the common mistake. If the callee's prototype disagrees with every caller, the prototype is what's wrong.

* Write code the way a developer in 1998 would have written it, not a line-by-line translation of the assembly.
** Use simple loops that index arrays. The compiler usually turns an indexed loop into pointer walking by itself, so pointer walking in the assembly doesn't always mean the source did it.
** Use decimal for counts, sizes and game values (`100`, `9999`). Use hex where a programmer would have: bit masks and addresses (`0xFF`, `0x8000`).
** Give values that mean something a name, as a #define or enum, instead of a bare number.
** Avoid `goto`. Try the structured forms first: `if`/`else`, a `while (1)` loop that ends with `break`, `return` or `continue`, or a ternary. If a match can't be found without a todo it can be okay. Readable and matching c code is better than nothing.
** Avoid inline `asm()`. That includes `asm("")` barriers and pinning a variable to a register (`register s32 x asm("$16")`). Look for a plain C version first. It's okay when the original clearly had inline asm, e.g. an instruction the compiler never generates. The PsyQ GTE macros (`gte_*` in `include/psxsdk/inline_c.h`) come from the SDK and are fine to use.

Example: `g_party` is an array of three `Player`s (the struct from above), and every member's level is set to 100.

Bad:
```
void maxPartyLevel(void) {
    Player *p = g_party;
    s32 i = 0;

loop:                     // BAD! A goto loop instead of a for loop
    p->level = 0x64;      // BAD! Hex for a game value, and no name for it
    p++;                  // BAD! Walks a pointer instead of indexing the array
    i++;
    if (i < 3) goto loop; // BAD! What is 3?
}
```

Good:
```
#define PARTY_SIZE 3
#define MAX_LEVEL 100

void maxPartyLevel(void) {
    s32 i;

    for (i = 0; i < PARTY_SIZE; i++) {
        g_party[i].level = MAX_LEVEL;
    }
}
```

* Documenting functions with Doxygen is encouraged, but not required. A `/** ... */` block saying what the function does, what its parameters are and what it returns helps the next person. If you're not sure, write your best guess and say that it is a guess.

Example:
```
/**
 * @brief Checks whether a party member is knocked out.
 * @param index Party slot, 0 to 2.
 * @return 1 if the member has 0 HP, otherwise 0.
 * @note Unsure whether this also counts Petrify.
 */
s32 isKnockedOut(s32 index) {
    ...
}
```

* Comments should say what the code itself can't, e.g. what a value means, or why code is written in an odd way because the match needs it.
** No comments that repeat the code.
** No status notes such as "the caller hasn't been decompiled yet". They go stale as soon as someone decompiles the next function, and nobody goes back to fix them.
** No notes after an `#include` saying what it's for. They go wrong as soon as the file uses something else from that header.

Bad:
```
#include "battle.h" // for g_party

// the caller hasn't been decompiled yet
void maxPartyLevel(void) {
    s32 unused;
    s32 i;

    for (i = 0; i < PARTY_SIZE; i++) {
        if (g_party[i].level == 0) { // if level is 0
            continue;
        }
        g_party[i].level = MAX_LEVEL;
    }
}
```
* `// for g_party` is wrong as soon as the file uses anything else from battle.h.
* `// the caller hasn't been decompiled yet` is wrong as soon as someone decompiles the caller.
* `// if level is 0` repeats the code.
* `s32 unused;` has no comment, so the next person will delete it and break the match.

Good:
```
#include "battle.h"

void maxPartyLevel(void) {
    s32 unused; // never used, but the compiled code only matches the original with it
    s32 i;

    for (i = 0; i < PARTY_SIZE; i++) {
        if (g_party[i].level == 0) { // level 0 means the slot is empty
            continue;
        }
        g_party[i].level = MAX_LEVEL;
    }
}
```
* `// never used, but ...` stops the next person from deleting the line.
* `// level 0 means the slot is empty` says what the value means, which the code alone can't.

* No copyrighted material in the repository. There are some assets integrated into the binaries, such as .TIM files that are just byte arrays as symbols. These have to be extracted during the extraction step and the c code generated.

* This one is obvious but perhaps more targeted towards llms: No hacks in the build. The build compiles the C, assembles and links it, nothing more. If the output doesn't match, fix the C, never the build.
** Never add a step that exists only to hide a mismatch, e.g. a script that patches bytes in the linked binary, edits one function's generated assembly, or links the original object file in place of the compiled one.
** Reason: a patched build still matches the original binary, but the C no longer describes the game's code, and that is the whole point of the project.
** Building a file with the compiler version and flags it was originally built with is fine (e.g. PsyQ 4.3 instead of 4.1 for some files). That's how the original was built, not a hack.

* Run `make full` to verify that the whole pipeline still builds and that the code matches before creating a pull request.