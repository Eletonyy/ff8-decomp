#!/usr/bin/env python3
"""Print decomp progress as a markdown table for updating the README."""

import glob
import os
import sys

# Overlay names in display order, with file extensions
MENU_OVERLAYS = [
    "menumain", "menucfg", "menupty", "menusts", "menuabl", "menushop",
    "menuext", "menuitem", "menumgc", "menugf", "menujnc2", "menusav",
    "menucrd", "menututo", "menutmag", "menutips", "menutest",
]
CODE_OVERLAYS = [
    "field_init", "intro", "field",
    "tripletriad", "battle_render", "battle", "world",
]

OVERLAY_EXT = {name: "ovl" for name in MENU_OVERLAYS}
OVERLAY_EXT.update({name: "bin" for name in CODE_OVERLAYS})

# C-source globs per overlay. Mirrors the Makefile's <name>_C_SRCS list.
# Defaults to src/ovl/<name>/*.c; override here when sources live elsewhere.
OVERLAY_SRC_GLOBS = {
    "intro":       ["src/intro.c", "src/intro_assets.c", "src/intro_state.c"],
    "field":       ["src/field/*.c"],
    "battle":      ["src/battle/*.c"],
    "tripletriad": ["src/tripletriad/*.c"],
    "world":       ["src/world/*.c"],
    **{name: [f"src/menu/{name}/*.c"] for name in MENU_OVERLAYS},
}


def count_functions_in_file(filepath):
    """Count C function definitions and INCLUDE_ASM macros in a .c file.

    Returns (c_count, asm_count).
    """
    with open(filepath) as f:
        content = f.read()

    asm_count = content.count("INCLUDE_ASM(")

    lines = content.split("\n")
    c_count = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Skip INCLUDE_ASM lines
        if "INCLUDE_ASM(" in stripped:
            i += 1
            continue

        # Skip lines not at column 0, empty, comments, preprocessor
        if not line or line[0] in " \t#/" or stripped.startswith(("/*", "*")):
            i += 1
            continue

        # Skip type/struct/extern definitions
        if stripped.startswith(("typedef ", "struct ", "enum ", "union ", "extern ")):
            i += 1
            continue

        # Look for function definitions: line has ( and is followed by {
        if "(" in stripped:
            found_brace = "{" in stripped
            found_semi = stripped.endswith(";")
            j = i + 1
            while not found_brace and not found_semi and j < min(i + 10, len(lines)):
                ns = lines[j].strip()
                if "{" in ns:
                    found_brace = True
                if ns.endswith(";") and "{" not in ns:
                    found_semi = True
                j += 1

            if found_brace and not found_semi:
                first_word = stripped.split()[0] if stripped.split() else ""
                if first_word not in (
                    "if", "else", "for", "while", "switch", "do", "return", "{", "}",
                ):
                    c_count += 1

        i += 1

    return c_count, asm_count



def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(root)

    rows = []

    # Collect overlay source paths first so we can exclude them from the main count.
    overlay_sources = {}
    for name in MENU_OVERLAYS + CODE_OVERLAYS:
        globs = OVERLAY_SRC_GLOBS.get(name, [f"src/ovl/{name}/*.c"])
        overlay_sources[name] = sorted(
            f for pattern in globs for f in glob.glob(pattern)
        )

    all_overlay_files = {f for files in overlay_sources.values() for f in files}

    # Main binary: src/*.c minus anything claimed by an overlay.
    c, asm = 0, 0
    for f in sorted(glob.glob("src/*.c")):
        if f in all_overlay_files:
            continue
        fc, fa = count_functions_in_file(f)
        c += fc
        asm += fa
    rows.append(("SLUS_008.92", c, asm, c + asm))

    # Overlays: count from their own source files.
    for name in MENU_OVERLAYS + CODE_OVERLAYS:
        c, asm = 0, 0
        for f in overlay_sources[name]:
            fc, fa = count_functions_in_file(f)
            c += fc
            asm += fa
        ext = OVERLAY_EXT[name]
        rows.append((f"{name}.{ext}", c, asm, c + asm))

    # Print markdown table
    print("| Binary | C | ASM | Total | % Decomped |")
    print("|--------|---|-----|-------|------------|")

    grand_c = 0
    grand_asm = 0
    grand_total = 0
    for name, c, asm, total in rows:
        pct = (c / total * 100) if total > 0 else 0
        print(f"| `{name}` | {c} | {asm} | {total} | {pct:.1f}% |")
        grand_c += c
        grand_asm += asm
        grand_total += total

    grand_pct = (grand_c / grand_total * 100) if grand_total > 0 else 0
    print(
        f"| **Total** | **{grand_c}** | **{grand_asm}** | "
        f"**{grand_total}** | **{grand_pct:.1f}%** |"
    )


if __name__ == "__main__":
    main()
