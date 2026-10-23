#!/bin/bash
# P-2j host test (not wired in CMake; MAC-W0 owns CMake): tests/portable/p2j_hanim_test.cpp + the REAL source/standalone/rw/hanim.cpp (same -D/-I/-isystem set as the PortableMac build of that TU, no PCH).
# Unresolved librw symbols (hierarchy/frame functions the test never calls) are resolved lazily (-undefined dynamic_lookup); RwStreamRead/Write are the test's memory stream.
# `mutate`: builds mutants of a copy of hanim.cpp (file stride 40 / wrong payload size / wrong GetSize / unscaled write offset) and requires every one to FAIL the test.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2j_test}"; mkdir -p "$OUT"
FLAGS=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/standalone/rw/hanim.cpp'))
c = shlex.split(e['command']); keep = []; i = 0
while i < len(c):
    a = c[i]
    if a in ('-o', '-MF', '-MT', '-MQ'): i += 2; continue
    if a == '-Xclang' and c[i+1] in ('-include', '-include-pch'): i += 4; continue
    if a in ('-c', '-MD', '-MMD', '-Winvalid-pch') or a.endswith('hanim.cpp'): i += 1; continue
    if a == '-O2': a = '-O1'
    keep.append(a); i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
build() {  # $1 = hanim.cpp to compile, $2 = exe
eval "nice -n 10 $FLAGS -g -fsanitize=address,undefined -Wl,-undefined,dynamic_lookup -o '$2' '$REPO/tests/portable/p2j_hanim_test.cpp' '$1'" 2>"$OUT/build.err" || { grep -m 20 error "$OUT/build.err"; return 1; }
}
SRC="$REPO/source/standalone/rw/hanim.cpp"
build "$SRC" "$OUT/p2j_hanim_test" || { echo "BUILD FAILED"; exit 1; }
"$OUT/p2j_hanim_test"; rc=$?
[ "${1:-}" = "mutate" ] && [ $rc -eq 0 ] || exit $rc
MUT=(
 'constexpr RwUInt32 kFileKeyFrameSize = 0x24;|constexpr RwUInt32 kFileKeyFrameSize = 0x28;'
 'frames[i].prev = \&frames[prevOffset / kFileKeyFrameSize];|frames[i].prev = \&frames[prevOffset / sizeof(rw::HAnimKeyFrame)];'
 'RwStreamRead(stream, \&frames[i].time, 0x20) != 0x20|RwStreamRead(stream, \&frames[i].time, 0x1C) != 0x1C'
 'RwStreamRead(stream, \&prevOffset, 4) != 4|false'
 '(frames[i].prev - frames) * kFileKeyFrameSize|(frames[i].prev - frames)'
 'return animation->numFrames * static_cast<RwInt32>(kFileKeyFrameSize);|return animation->numFrames * static_cast<RwInt32>(sizeof(rw::HAnimKeyFrame));'
 'RwStreamWrite(stream, \&frames[i].time, 0x20)|RwStreamWrite(stream, \&frames[i].time, 0x1C)'
)
n=0; caught=0
for m in "${MUT[@]}"; do
  IFS='|' read -r old new <<<"$m"; n=$((n+1)); cp "$SRC" "$OUT/hanim_mut.cpp"
  python3 - "$OUT/hanim_mut.cpp" "$old" "$new" <<'PY' || { echo "mutant $n: pattern not found"; continue; }
import sys
p, old, new = sys.argv[1:4]
old = old.replace('\\&', '&'); new = new.replace('\\&', '&')
s = open(p).read(); assert old in s, old
open(p, 'w').write(s.replace(old, new, 1))
PY
  # a mutant copy lives outside source/: quoted includes resolve through the -I set
  if build "$OUT/hanim_mut.cpp" "$OUT/mut" >/dev/null 2>&1; then if "$OUT/mut" >/dev/null 2>&1; then echo "mutant $n SURVIVED: $old"; else caught=$((caught+1)); fi; else caught=$((caught+1)); fi
done
echo "mutants caught: $caught/$n"; [ $caught -eq $n ]
