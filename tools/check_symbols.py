#!/usr/bin/env python3
"""Symbol-hygiene checker for the ff8-decomp tree.

Enforces the "one declaration, one type, one name" invariant described in
cleanuptask.md. This is hygiene only; it never changes build output. Run it
manually (or via `make check`) to audit the tree and gate a subsystem before
moving on.

It exits non-zero when it finds any of:
  A. A file-scope `extern` (data or function) in a src/**/*.c.
  B. An `extern` inside a function body.
  C. A typedef/struct/union *defined* in a .c file.
  D. The same physical symbol (address) declared with different names across
     the splat configs (symbol_addrs.* / undefined_syms*.*).
  E. The same data symbol declared with conflicting types across headers.
  F. The same function prototype declared in more than one header within a
     scope (R1: one prototype, one owning header). Prototypes that differ only
     in argument names count as duplicates; genuinely divergent signatures
     (the matching-driven exceptions) are reported separately as [F:divergent]
     and are not failures.

Scope / overlay constraint: overlays load to the same address range, so a
symbol at a given address in one overlay is a different object than the same
address in another. Name-consistency (D) is therefore checked per *config
group*: the main binary (symbol_addrs.txt) plus, for each overlay, that
overlay's own configs. See cleanuptask.md for the full rationale.

Allowed exception: a file-scope `extern NAME ...;` directly preceded by
`#undef NAME` is a deliberate local shadow of a header macro (e.g. world.h
exposes some symbols both as an OTSlot-array macro and, after #undef, as a
flat byte array). These are reported as INFO, not failures.
"""
import os
import re
import sys
import glob
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

EXTERN_RE = re.compile(r'^\s*extern\b')
UNDEF_RE = re.compile(r'^\s*#undef\s+([A-Za-z_]\w*)')
TYPEDEF_RE = re.compile(r'^\s*typedef\b')
# a struct/union *definition* (has a body), not a forward decl or a var of
# struct type: `struct Foo {` / `union Bar {` at file scope.
STRUCT_DEF_RE = re.compile(r'^\s*(?:typedef\s+)?(?:struct|union)\b[^;{]*\{')

CFG_DIR = os.path.join(ROOT, 'config')


def c_sources():
    return sorted(glob.glob(os.path.join(ROOT, 'src', '**', '*.c'), recursive=True))


def strip_line_comment(s):
    # crude: drop // comments (good enough for the structural checks here)
    i = s.find('//')
    return s[:i] if i >= 0 else s


def overlay_of(path):
    """Map a src path to its link-unit key for reporting."""
    rel = os.path.relpath(path, ROOT)
    parts = rel.split(os.sep)
    if parts[0] == 'src' and len(parts) >= 3:
        if parts[1] == 'ovl':
            return parts[2]
        return parts[1]
    return 'main'


# ---------------------------------------------------------------------------
# Checks A / B / C — per .c structural scan
# ---------------------------------------------------------------------------

def scan_c_files():
    fails = []
    infos = []
    for path in c_sources():
        rel = os.path.relpath(path, ROOT)
        try:
            lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
        except OSError:
            continue
        depth = 0           # brace depth (rough; ignores braces in strings)
        in_block_comment = False
        # names #undef'd at file scope earlier in this file — a following
        # file-scope `extern NAME` is then an allowed header-macro shadow.
        undeffed = set()
        for i, raw in enumerate(lines, 1):
            line = raw
            # track block comments coarsely
            if in_block_comment:
                if '*/' in line:
                    in_block_comment = False
                    line = line.split('*/', 1)[1]
                else:
                    continue
            # remove inline block comments
            while '/*' in line:
                if '*/' in line.split('/*', 1)[1]:
                    pre, rest = line.split('/*', 1)
                    line = pre + rest.split('*/', 1)[1]
                else:
                    line = line.split('/*', 1)[0]
                    in_block_comment = True
                    break
            code = strip_line_comment(line)

            mu = UNDEF_RE.match(code)
            if mu and depth == 0:
                undeffed.add(mu.group(1))

            stripped = code.strip()

            if EXTERN_RE.match(code):
                # Check B: inside a function body?
                if depth > 0:
                    fails.append(f"[B] {rel}:{i}: extern inside function body: {stripped}")
                else:
                    # Allowed #undef-shadow exception?
                    m = re.match(r'^\s*extern\s+[^;]*?\b([A-Za-z_]\w*)\s*(\[|;|\()', code)
                    name = m.group(1) if m else None
                    if name and name in undeffed:
                        infos.append(f"[A:ok] {rel}:{i}: #undef-shadow extern allowed: {stripped}")
                    else:
                        fails.append(f"[A] {rel}:{i}: file-scope extern in .c: {stripped}")

            # Check C: typedef / struct-union definition at file scope
            if depth == 0:
                if TYPEDEF_RE.match(code) or STRUCT_DEF_RE.match(code):
                    fails.append(f"[C] {rel}:{i}: type defined in .c (move to a header): {stripped}")

            # update brace depth on the code-only view
            depth += code.count('{') - code.count('}')
            if depth < 0:
                depth = 0
    return fails, infos


# ---------------------------------------------------------------------------
# Check D — name consistency across configs (per link-unit group)
# ---------------------------------------------------------------------------

CFG_LINE_RE = re.compile(r'^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;')


def parse_cfg(path):
    out = []
    if not os.path.exists(path):
        return out
    for ln in open(path, errors='replace').read().splitlines():
        m = CFG_LINE_RE.match(ln)
        if m:
            out.append((m.group(1), int(m.group(2), 16)))
    return out


def overlay_names():
    ovls = set()
    for p in glob.glob(os.path.join(CFG_DIR, 'symbol_addrs.*.txt')):
        ovls.add(os.path.basename(p)[len('symbol_addrs.'):-len('.txt')])
    return sorted(ovls)


def check_name_consistency():
    """For each config group (main + each overlay's own files), an address must
    map to a single name."""
    fails = []
    groups = {}
    # main binary
    groups['main'] = [os.path.join(CFG_DIR, 'symbol_addrs.txt')]
    for ovl in overlay_names():
        files = [
            os.path.join(CFG_DIR, f'symbol_addrs.{ovl}.txt'),
            os.path.join(CFG_DIR, f'undefined_syms.{ovl}.txt'),
            os.path.join(CFG_DIR, f'undefined_syms_auto.{ovl}.txt'),
        ]
        # an overlay also resolves main-binary imports; include the main file
        files.append(os.path.join(CFG_DIR, 'symbol_addrs.txt'))
        groups[ovl] = files

    for grp, files in sorted(groups.items()):
        addr_names = defaultdict(set)
        for f in files:
            for name, addr in parse_cfg(f):
                addr_names[addr].add(name)
        for addr, names in sorted(addr_names.items()):
            if len(names) > 1:
                # a D_<addr> auto-name coexisting with a friendly name is the
                # common, fixable case; report all multi-name addresses.
                fails.append(
                    f"[D] {grp}: address 0x{addr:08X} has {len(names)} names: "
                    + ", ".join(sorted(names)))
    return fails


# ---------------------------------------------------------------------------
# Check E — conflicting data-symbol types across headers
# ---------------------------------------------------------------------------

HDR_EXTERN_RE = re.compile(r'^\s*extern\s+(.+?);')


def norm_type(t):
    return re.sub(r'\s+', ' ', t.replace('*', ' * ')).strip()


def check_header_type_conflicts():
    fails = []
    decls = defaultdict(set)   # sym -> set((header, normtype))
    for h in glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True):
        rel = os.path.relpath(h, ROOT)
        for ln in open(h, errors='replace').read().splitlines():
            m = HDR_EXTERN_RE.match(ln)
            if not m:
                continue
            body = m.group(1)
            if '(' in body and ')' in body:   # function proto / fn ptr
                continue
            # take first declarator
            mm = re.match(r'^(.*?)([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*$', body.split(',')[0])
            if not mm:
                continue
            base, sym, arr = mm.group(1).strip(), mm.group(2), mm.group(3) or ''
            decls[sym].add((rel, norm_type(base + ' ' + arr)))
    for sym, ds in sorted(decls.items()):
        types = {t for _, t in ds}
        if len(types) > 1:
            locs = "; ".join(f"{h}: {t}" for h, t in sorted(ds))
            fails.append(f"[E] {sym}: {len(types)} conflicting header types -> {locs}")
    return fails


# ---------------------------------------------------------------------------
# Check F — duplicate function prototypes across headers
# ---------------------------------------------------------------------------

HDR_PROTO_RE = re.compile(r'^\s*extern\s+(.+?\([^;]*\))\s*;')


def header_scope(rel):
    """Resolve a header to its link-unit scope. Overlay-local headers live in
    include/<overlay>/; func_8009XXXX names mean different things in different
    overlays, so duplicate detection is scoped per directory."""
    parts = rel.split('/')
    if parts[0] == 'include' and len(parts) >= 3:
        return parts[1]          # 'world', 'field', 'psxsdk', ...
    return 'shared'              # include/*.h (battle.h, world.h, common.h, ...)


def split_proto(sig):
    """Return (name, return_type, (param_types,...)) with parameter *names*
    stripped, so prototypes that differ only in argument names compare equal."""
    lp = sig.index('(')
    rp = sig.rindex(')')
    head = sig[:lp].strip()
    m = re.match(r'^(.*?)([A-Za-z_]\w*)$', head)
    ret, name = (m.group(1).strip(), m.group(2)) if m else ('', head)
    params = sig[lp + 1:rp].strip()
    ptypes = []
    if params and params != 'void':
        for p in params.split(','):
            p = p.strip()
            pm = re.match(r'^(.*?)([A-Za-z_]\w*)\s*(\[\s*\])?$', p)
            if pm and pm.group(1).strip():
                ptypes.append(norm_type(pm.group(1).strip() + (pm.group(3) or '')))
            else:
                ptypes.append(norm_type(p))   # unnamed param: keep as-is
    elif params == 'void':
        ptypes = ['void']
    return name, norm_type(ret), tuple(ptypes)


def check_duplicate_prototypes():
    """Flag functions whose prototype is declared in more than one header
    within the same scope. Identical duplicates are R1 violations (consolidate
    into the definer's header); divergent ones are the matching-driven
    exceptions and are reported separately, not as failures."""
    fails, infos = [], []
    # scope -> funcname -> list of (header, (ret, params))
    seen = defaultdict(lambda: defaultdict(list))
    for h in glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True):
        rel = os.path.relpath(h, ROOT)
        scope = header_scope(rel)
        if scope in ('psxsdk',):           # SDK headers own their decls
            continue
        for ln in open(h, errors='replace').read().splitlines():
            m = HDR_PROTO_RE.match(ln)
            if not m:
                continue
            sig = re.sub(r'\s+', ' ', m.group(1)).strip()
            if '(*' in sig or '(' not in sig:   # skip function pointers
                continue
            try:
                name, ret, params = split_proto(sig)
            except ValueError:
                continue
            if not re.match(r'^(func_[0-9A-Fa-f]+|[A-Za-z_]\w*)$', name):
                continue
            seen[scope][name].append((rel, (ret, params)))
    for scope in sorted(seen):
        for name, decls in sorted(seen[scope].items()):
            headers = sorted({h for h, _ in decls})
            if len(headers) < 2:
                continue
            shapes = {sh for _, sh in decls}
            if len(shapes) == 1:
                fails.append(f"[F] {scope}: {name} prototype duplicated in "
                             + ", ".join(headers)
                             + " (consolidate into the definer's header)")
            else:
                variants = "; ".join(
                    f"{h}: {ret} ({', '.join(p)})" for h, (ret, p) in sorted(decls))
                infos.append(f"[F:divergent] {scope}: {name} declared with "
                             f"{len(shapes)} signatures -> {variants}")
    return fails, infos


# ---------------------------------------------------------------------------

def main():
    only = sys.argv[1:] if len(sys.argv) > 1 else None
    af, infos = scan_c_files()
    df = check_name_consistency()
    ef = check_header_type_conflicts()
    ff, finfos = check_duplicate_prototypes()
    infos = infos + finfos

    all_fails = af + df + ef + ff
    if only:
        all_fails = [f for f in all_fails if any(o in f for o in only)]
        infos = [i for i in infos if any(o in i for o in only)]

    for line in infos:
        print(line)
    for line in all_fails:
        print(line)

    print()
    print(f"check_symbols: {len(af)} structural (.c), {len(df)} name, "
          f"{len(ef)} header-type, {len(ff)} duplicate-prototype findings; "
          f"{len(infos)} allowed exceptions")
    return 1 if all_fails else 0


if __name__ == '__main__':
    sys.exit(main())
