#!/bin/bash
# P-2f host test (not wired in CMake; MAC-W0 owns CMake): tests/portable/p2f_radar_test.cpp
#  - goldens (tRadarTrace_File 0x28: basic / every bit-field once / extreme) come from tools/portable/gen_x86_golden.py (independent of the code under test)
#  - REAL source/game_sa/Radar.h (cut before `class CRadar`, RGBA.h / Vector2D.h includes dropped: they pull the whole engine), REAL FileRecords/RadarFile.hpp / FileRecord.hpp / GenericGameStorage.h,
#    and the REAL CRadar::Load / CRadar::Save bodies extracted verbatim from Radar.cpp (compiled against a stub pool)
#  - `mutate` also builds mutants of the generated copies and requires every one to FAIL the test
# Compile flags (-D set, std, arch) come from build/PortableMac/compile_commands.json.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2f_test}"; mkdir -p "$OUT/inc/FileRecords"
S="$REPO/source"
{ echo '#include <cstdint>'; python3 "$REPO/tools/portable/gen_x86_golden.py" cpp tRadarTrace_File; } > "$OUT/p2f_golden.inc" || exit 1
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
gen() {  # $1 = output dir; copies/extracts the code under test
python3 - "$REPO" "$1" <<'PY'
import sys, os, re
repo, out = sys.argv[1:3]
os.makedirs(out + '/inc/FileRecords', exist_ok=True)
h = open(repo + '/source/game_sa/Radar.h').read().replace('#include "RGBA.h"\n', '').replace('#include "Vector2D.h"\n', '')
open(out + '/inc/Radar.h', 'w').write(h[:h.index('static constexpr uint32 MAX_RADAR_WIDTH_TILES')])
open(out + '/inc/FileRecords/RadarFile.hpp', 'w').write(open(repo + '/source/game_sa/FileRecords/RadarFile.hpp').read())
c = open(repo + '/source/game_sa/Radar.cpp').read()
a = c.index('bool CRadar::Load() {'); b = c.index('bool CRadar::Save() {'); e = c.index('\n}\n', b) + 3
assert c.index('\n}\n', a) + 3 < b
open(out + '/p2f_saveload.inc', 'w').write(c[a:e])
PY
}
build() {  # $1 = work dir, $2 = exe
eval "nice -n 10 $BASE -I'$1/inc' -I'$REPO/tests/portable/p2c_stubs' -I$S/game_sa/Core -I$S/game_sa -I$S/game_sa/Enums -I$S -I'$1' -I'$OUT' -w -fsanitize=address,undefined -g -DREPO_DIR=$REPO -o '$2' '$REPO/tests/portable/p2f_radar_test.cpp'" 2>"$1/build.err" || { head -40 "$1/build.err"; return 1; }
}
gen "$OUT" || exit 1
build "$OUT" "$OUT/p2f_radar_test" || { echo "BUILD FAILED"; exit 1; }
"$OUT/p2f_radar_test"; rc=$?
[ "${1:-}" = "mutate" ] || exit $rc
[ $rc -eq 0 ] || exit $rc
# ---- mutants: each edit of the generated copy must make the test fail ----------------------------------------------------------------------------------------------------------
MUT=(
 'inc/Radar.h|t.m_nBlipSprite   = (eRadarSprite)f.blipSprite;|t.m_nBlipSprite = (eRadarSprite)0;'
 'inc/Radar.h|f.entryExitPoolInd = entryExitPoolIndPlusOne;|f.entryExitPoolInd = 0;'
 'inc/Radar.h|<< kRadarCoordAppearanceShift));|<< 5));'
 'inc/Radar.h|<< kRadarTypeShift)|<< 3)'
 'inc/Radar.h|(t.m_bBlipFade ? kRadarFlagBlipFade : 0)|0'
 'inc/Radar.h|f.counter          = t.m_nCounter;|f.counter = 0;'
 'inc/Radar.h|t.m_nBlipType        = (eBlipType)((f.flags1 >> kRadarTypeShift) & kRadarTypeMask);|t.m_nBlipType = (eBlipType)((f.flags1 >> kRadarTypeShift) \& 7);'
 'inc/FileRecords/RadarFile.hpp|kRadarFlagFriendly = 1u << 3|kRadarFlagFriendly = 1u << 4'
 'p2f_saveload.inc|enexInd = (uint32)(index + 1);|enexInd = (uint32)index;'
 'p2f_saveload.inc|copy.m_bTrackingBlip = false;|(void)0;'
 'p2f_saveload.inc|trace.m_pEntryExit = CEntryExitManager::GetInSlot(rec.entryExitPoolInd - 1);|trace.m_pEntryExit = CEntryExitManager::GetInSlot(rec.entryExitPoolInd);'
 'p2f_saveload.inc|!CEntryExitManager::GetPool()->IsFreeSlotAtIndex(index)|true'
 'p2f_saveload.inc|auto copy = trace;|auto\& copy = trace;'
)
n=0; caught=0
for m in "${MUT[@]}"; do
  IFS='|' read -r f old new <<<"$m"; n=$((n+1)); W="$OUT/mut"; rm -rf "$W"; mkdir -p "$W"; gen "$W"
  python3 - "$W/$f" "$old" "$new" <<'PY' || { echo "mutant $n: pattern not found"; continue; }
import sys
p, old, new = sys.argv[1:4]
s = open(p).read(); assert old in s, old
open(p, 'w').write(s.replace(old, new, 1))
PY
  if build "$W" "$W/t" >/dev/null 2>&1; then if "$W/t" >/dev/null 2>&1; then echo "mutant $n SURVIVED: $f: $old"; else caught=$((caught+1)); fi; else caught=$((caught+1)); fi   # build break counts as caught
done
echo "mutants caught: $caught/$n"; [ $caught -eq $n ]
