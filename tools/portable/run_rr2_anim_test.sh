#!/bin/bash
# MAC-RR2 host test: tests/portable/rr2_setcurrentanim_test.cpp + the REAL source/standalone/rw/rtanim.cpp (same flags as the PortableMac build of that TU, no PCH) + librw.a.
# `mutate`: the NOTSA_PORTABLE guard that stops SetCurrentAnim storing key-frame pointers over the game's 0x1C-byte {q,t} interp frames is removed -> the test must FAIL.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/rr2_test}"; mkdir -p "$OUT"
FLAGS=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/standalone/rw/rtanim.cpp'))
c = shlex.split(e['command']); keep = []; i = 0
while i < len(c):
    a = c[i]
    if a in ('-o', '-MF', '-MT', '-MQ'): i += 2; continue
    if a == '-Xclang' and c[i+1] in ('-include', '-include-pch'): i += 4; continue
    if a in ('-c', '-MD', '-MMD', '-Winvalid-pch') or a.endswith('rtanim.cpp'): i += 1; continue
    if a == '-O2': a = '-O1'
    keep.append(a); i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
BDIR="$(dirname "$COMPDB")"
# the link line of the game (librw.a drags the GL3 backend, so SDL3 / OpenGL / frameworks are needed to link even though this test never calls them)
LIBS=$(python3 - "$BDIR" <<'PY'
import re, sys
t = open(sys.argv[1] + '/build.ninja').read()
m = re.search(r'^build bin/gta_reversed:.*?\n  LINK_LIBRARIES = (.*?)\n', t, re.S | re.M)
print(m.group(1).replace('vendor/librw/src/librw.a', sys.argv[1] + '/vendor/librw/src/librw.a'))
PY
)
WEAKFW=""; [ "$(uname)" = Darwin ] && WEAKFW="-Wl,-weak_framework,CoreHaptics"
build() {  # $1 = rtanim.cpp to compile, $2 = exe
eval "nice -n 10 $FLAGS -g -fsanitize=address,undefined -o '$2' '$REPO/tests/portable/rr2_setcurrentanim_test.cpp' '$1' $LIBS $WEAKFW" 2>"$OUT/build.err" || { grep -m 20 -i "error" "$OUT/build.err"; return 1; }
}
SRC="$REPO/source/standalone/rw/rtanim.cpp"
build "$SRC" "$OUT/rr2_test" || { echo "BUILD FAILED"; exit 1; }
"$OUT/rr2_test"; rc=$?
[ "${1:-}" = "mutate" ] && [ $rc -eq 0 ] || exit $rc
cp "$SRC" "$OUT/rtanim_mut.cpp"
python3 - "$OUT/rtanim_mut.cpp" <<'PY' || { echo "mutant: pattern not found"; exit 1; }
import sys
p = sys.argv[1]; s = open(p).read()
old = "if (info->id != kAnimBlendSchemeID)"; assert old in s
open(p, 'w').write(s.replace(old, "if (true)", 1))
PY
if build "$OUT/rtanim_mut.cpp" "$OUT/mut" >/dev/null 2>&1; then if "$OUT/mut" >/dev/null 2>&1; then echo "mutant SURVIVED"; exit 1; fi; fi
echo "mutant caught"
