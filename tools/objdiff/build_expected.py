#!/usr/bin/env python3
"""Build expected/ -- one object per C unit holding the original code.

objdiff diffs a "base" object (what we compiled) against a "target" object
(what the code should be). The progress report gets by with the built object
on both sides, because every INCLUDE_ASM function carries a .NON_MATCHING
marker; but the objdiff GUI needs a real target to show a diff for a function
that does not match yet. This assembles that target for every C unit from
splat's disassembly, which covers every function once `disassemble_all` is
on: asm/.../matchings/<unit>/*.s for the decompiled ones,
asm/.../nonmatchings/<unit>/*.s for the rest, in address order, plus the
unit's own data files when the link does not carry them as separate objects.
Each object lands at expected/<build_path>/<src_path>/<unit>.o, mirroring
build/, which is where objdiff_generate.py looks.

Run it on a tree that passes `make verify` (the disassembly must be the one
the build was split from), and again after `make split` moves any segment.

Usage:
  build_expected.py [--check] [binary ...]

--check relinks each binary from the expected objects and compares the
result's SHA1 with the original: that proves the objects are the original
code, where a byte comparison of the objects would not (the C build and the
disassembly spell the same relocation differently).
"""

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent.parent
SPLAT_GEN = ROOT / "build" / "splat"
EXPECTED = ROOT / "expected"
# The build's assembler flags, plus -W: splat's SDK stubs lack .end directives.
AS = ["mipsel-linux-gnu-as", "-march=r3000", "-mabi=32", "-EL", "-no-pad-sections", "-O0", "-Iinclude", "-W"]
OBJCOPY = "mipsel-linux-gnu-objcopy"
LD = "mipsel-linux-gnu-ld"
MAIN = "SLUS_008.92"
DATA_SECTIONS = ("rodata", "data", "sdata", "bss", "sbss")

ADDR_RE = re.compile(r"/\* [0-9A-Fa-f]+ ([0-9A-Fa-f]{8})(?: [0-9A-Fa-f]{8})? \*/")
LABEL_RE = re.compile(r"^(glabel|alabel|jlabel|ehlabel|dlabel) (\S+)", re.M)
SECTION_OF = {"T": ".text", "t": ".text", "D": ".data", "d": ".data", "R": ".rodata", "r": ".rodata",
              "B": ".bss", "b": ".bss", "S": ".sbss", "s": ".sbss", "G": ".sdata", "g": ".sdata"}


def c_units(config):
    """The objects the build compiles from C for this binary, as unit names
    relative to src_path (e.g. `bc_object15`, `psxsdk/crt0`), in link order.
    Data the build generates as C (assets) counts too: its symbols come from
    the disassembly's data files instead."""
    opts = config["options"]
    prefix = f"{opts['build_path']}/{opts['src_path']}/"
    for obj in linked_objects(config):
        if obj.startswith(prefix):
            yield obj[len(prefix):-2]


def function_address(path):
    """vram of the function's first instruction: the first address comment
    after its glabel (a jump table migrated into the file comes first and
    would sort the function by its rodata address)."""
    seen_label = False
    first = None
    with open(path) as f:
        for line in f:
            if line.startswith(("glabel ", "alabel ", "jlabel ", "ehlabel ")):
                seen_label = True
                continue
            m = ADDR_RE.search(line)
            if not m:
                continue
            if seen_label:
                return int(m.group(1), 16)
            if first is None:
                first = int(m.group(1), 16)
    if first is None:
        raise ValueError(f"{path}: no instruction address comment")
    return first


def without_migrated_rodata(text):
    """A function file minus the rodata block splat migrated in ahead of it."""
    out = []
    in_rodata = False
    for line in text.split("\n"):
        if line.startswith(".section .rodata"):
            in_rodata = True
            continue
        if in_rodata and (line.startswith(".section .text") or line.startswith(".text")):
            in_rodata = False
            continue
        if not in_rodata:
            out.append(line)
    return "\n".join(out) + "\n"


def built_sections(built):
    """Symbol -> section of the built object, from nm."""
    if not built.exists():
        return {}
    out = subprocess.run(["mipsel-linux-gnu-nm", str(built)], capture_output=True, text=True, check=True).stdout
    sections = {}
    for line in out.split("\n"):
        parts = line.split()
        if len(parts) == 3 and parts[1] in SECTION_OF:
            sections[parts[2]] = SECTION_OF[parts[1]]
    return sections


def data_blob(text, section):
    """A data file from the function directory (a table inside the code
    range), re-homed to the section the build gives it."""
    lines = [l for l in text.split("\n") if not l.startswith(".section")]
    return f".section {section}\n" + "\n".join(lines) + "\n"


def linked_objects(config):
    """Objects splat's dependency file says the link takes, in link order."""
    dep = (ROOT / config["options"]["ld_script_path"]).with_suffix(".d")
    if not dep.exists():
        return []
    # The dependency file names an object once per section it contributes.
    return list(dict.fromkeys(tok for tok in dep.read_text().replace("\\\n", " ").split() if tok.endswith(".o")))


def build_unit(config, unit):
    opts = config["options"]
    asm_path = ROOT / opts["asm_path"]
    build_path = Path(opts["build_path"])
    src_path = Path(opts["src_path"])

    functions = []
    for kind in ("matchings", "nonmatchings"):
        d = asm_path / kind / unit
        if d.is_dir():
            functions += [p for p in d.iterdir() if p.suffix == ".s"]
    functions.sort(key=function_address)

    # The unit's data comes from splat's data files, minus any the link
    # takes as objects of their own. Jump tables that splat migrated into
    # the function files are dropped from those, so each is defined once
    # and the rodata keeps the original order.
    linked = linked_objects(config)
    data = []
    for sect in DATA_SECTIONS:
        s_file = asm_path / "data" / f"{unit}.{sect}.s"
        if not s_file.exists():
            continue
        as_object = str(build_path / s_file.relative_to(ROOT).with_suffix(".o"))
        if as_object in linked:
            continue
        data.append(s_file)
    if not functions and not data:
        return None

    out = EXPECTED / build_path / src_path / f"{unit}.o"
    out.parent.mkdir(parents=True, exist_ok=True)
    built = ROOT / build_path / src_path / f"{unit}.o"
    sections = built_sections(built)
    # Symbols the unit's data files already carry, in their original order.
    in_data_files = set()
    for s_file in data:
        in_data_files.update(m.group(2) for m in LABEL_RE.finditer(s_file.read_text()))
    wrapper = out.with_suffix(".s")
    with open(wrapper, "w") as f:
        f.write('.include "macro.inc"\n.set noat\n.set noreorder\n')
        for fn in functions:
            text = fn.read_text()
            labels = LABEL_RE.findall(text)
            if labels and all(kind == "dlabel" for kind, _ in labels):
                # A table splat found inside the code range. It is taken from
                # here only when no data file carries it and the build defines
                # it from C, in the section the build gives it.
                name = labels[0][1]
                section = sections.get(name)
                if section and name not in in_data_files:
                    f.write(data_blob(text, section))
                continue
            f.write(".text\n.align 2\n")
            f.write(without_migrated_rodata(text))
        for s_file in data:
            f.write(f'.include "{s_file.relative_to(ROOT)}"\n')

    subprocess.run(AS + ["-o", str(out), str(wrapper)], cwd=ROOT, check=True)
    # The .NON_MATCHING aliases mark unmatched functions in the *base*; the
    # target is the original code and carries none.
    subprocess.run([OBJCOPY, "--wildcard", "--strip-symbol=*.NON_MATCHING", str(out)], cwd=ROOT, check=True)
    wrapper.unlink()
    return out


def check_link(config, expected_objects):
    """Relink the binary with the expected objects in place of the built C
    units, the way the Makefile links it, and compare against the original."""
    opts = config["options"]
    name = config["name"]
    swap = {str(built.relative_to(ROOT)): str(exp.relative_to(ROOT)) for built, exp in expected_objects}
    objects = [swap.get(o, o) for o in linked_objects(config)]

    link_dir = EXPECTED / "link"
    link_dir.mkdir(parents=True, exist_ok=True)
    ld_script = ROOT / opts["ld_script_path"]
    script = ld_script.read_text()
    for built, exp in swap.items():
        script = script.replace(built, exp)
    ld_copy = link_dir / ld_script.name
    ld_copy.write_text(script)

    cmd = [LD, "-T", str(ld_copy),
           "-T", opts["undefined_funcs_auto_path"], "-T", opts["undefined_syms_auto_path"]]
    if name != MAIN:
        cmd += ["-T", "config/symbols.extern.txt"]
    elf = link_dir / f"{name}.elf"
    binary = link_dir / f"{name}.bin"
    subprocess.run(cmd + ["--no-check-sections", "-o", str(elf)] + objects, cwd=ROOT, check=True)
    subprocess.run([OBJCOPY, "-O", "binary", str(elf), str(binary)], check=True)
    built_sha = hashlib.sha1(binary.read_bytes()).hexdigest()
    original_sha = hashlib.sha1((ROOT / opts["target_path"]).read_bytes()).hexdigest()
    return built_sha == original_sha


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("binary", nargs="*", help="splat config names to build (default: every built one)")
    ap.add_argument("--check", action="store_true", help="relink each binary from the expected objects and compare with the original")
    args = ap.parse_args()

    configs = sorted(SPLAT_GEN.glob("*.yaml"))
    if args.binary:
        configs = [SPLAT_GEN / f"{b}.yaml" for b in args.binary]

    built = 0
    failed = []
    for cfg_path in configs:
        config = yaml.safe_load(cfg_path.read_text())
        opts = config["options"]
        if not (ROOT / opts["build_path"]).exists():
            continue
        objects = []
        for unit in c_units(config):
            out = build_unit(config, unit)
            if out is None:
                continue
            objects.append((ROOT / opts["build_path"] / opts["src_path"] / f"{unit}.o", out))
        built += len(objects)
        if args.check and objects:
            ok = check_link(config, objects)
            print(f"  {config['name']:<16} {len(objects):3d} objects  {'Match' if ok else 'MISMATCH'}")
            if not ok:
                failed.append(config["name"])
    print(f"expected/: {built} objects")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
