#!/bin/bash
# P-2d host test (not wired in CMake; MAC-W0 owns CMake): tests/portable/p2d_ped_test.cpp, REAL Weapon.h / PedSaveStructure.h / GenericGameStorage.h / FileRecord.hpp on the p2c Base.h/rwplcore.h stub (StdInc is not linkable on the Mac yet).
#  - golden vectors (CWeapon_File 0x1C, CPedSaveStructure_File 0x18C) come from tools/portable/gen_x86_golden.py (independent of the code under test)
#  - ToFile == golden byte-for-byte, FromFile(ToFile(x)) == x, 13-slot round trip, fx pointer slot 0, typed Save/Load = the 0x18C record, source scan of Ped.cpp (prefix = FileSizeOf)
# Compile flags (-D set, std, arch) come from build/PortableMac/compile_commands.json; the include set is the stub dir tests/portable/p2c_stubs + the real source dirs.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2d_test}"; mkdir -p "$OUT"
{ echo '#include <cstdint>'; python3 "$REPO/tools/portable/gen_x86_golden.py" cpp CWeapon_File CPedSaveStructure_File; } > "$OUT/p2d_golden.inc" || exit 1
BASE=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/game_sa/Pickups.cpp'))
c = shlex.split(e['command']); keep = [c[0]]; i = 1
while i < len(c):
    a = c[i]
    if a in ('-arch', '-isysroot'): keep += [a, c[i+1]]; i += 2; continue
    if a.startswith('-D') or a.startswith('-std=') or a.startswith('-stdlib=') or a in ('-fms-extensions', '-mms-bitfields', '-fsigned-char', '-fwrapv', '-fno-delayed-template-parsing'): keep.append(a)
    i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
S="$REPO/source"
eval "nice -n 10 $BASE -I'$REPO/tests/portable/p2c_stubs' -I$S/game_sa/Core -I$S/game_sa -I$S/game_sa/Enums -I$S -I'$OUT' -w -DNOTSA_P2D_HOSTTEST_NO_PED_H -DREPO_DIR=$REPO -o '$OUT/p2d_ped_test' '$REPO/tests/portable/p2d_ped_test.cpp'" 2>"$OUT/build.err" || { head -40 "$OUT/build.err"; echo "BUILD FAILED"; exit 1; }
grep -c warning "$OUT/build.err" | sed 's/^/build warnings: /'
"$OUT/p2d_ped_test"
