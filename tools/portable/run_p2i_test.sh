#!/bin/bash
# P-2i host test (not wired in CMake; MAC-W0 owns CMake): tests/portable/p2i_breakable_test.cpp, REAL BreakablePlugin.cpp / .h on tests/portable/p2i_stubs/StdInc.h.
#  - golden vectors (BreakInfo_File 0x34: exe / port / max) from tools/portable/gen_x86_golden.py (independent of the code under test)
#  - REAL DATA: every 0xFD chunk of every geometry extension of the DFFs in $GTA/models/gta3.img (never copied/committed): consumed == chunk length, blob layout, round trip;
#    its STATS line must equal the independent Python census tools/portable/p2i_scan_img.py
#  - built with ASan + UBSan (misaligned loads / heap overruns of the blob are errors)
#  - `run_p2i_test.sh mutate` additionally builds source-mutated copies (in $OUT/mut) and demands that the test FAILS on each
# Compile flags (-D set, std, arch) come from build/PortableMac/compile_commands.json.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2i_test}"; mkdir -p "$OUT"
GTA="${GTA:-$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas}"
export GTA_IMG="${GTA_IMG:-$GTA/models/gta3.img}"
[ -f "$GTA_IMG" ] || { echo "NOTE: $GTA_IMG missing: real-data part will be skipped"; unset GTA_IMG; }
{ echo '#include <cstdint>'; python3 "$REPO/tools/portable/gen_x86_golden.py" cpp BreakInfo_File; } > "$OUT/p2i_golden.inc" || exit 1
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
build() { # $1 = extra include dir (mutated copy) or "", $2 = output exe
  local MUT=""; [ -n "$1" ] && MUT="-I'$1'"
  eval "nice -n 10 $BASE -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer $MUT -I'$REPO/tests/portable/p2i_stubs' -I'$REPO/tests/portable/p2c_stubs' -I$S/game_sa/Core -I$S/game_sa -I$S/game_sa/Enums -I$S -I'$OUT' -w -o '$2' '$REPO/tests/portable/p2i_breakable_test.cpp'" 2>"$2.err"
}
export ASAN_OPTIONS=detect_leaks=0:symbolize=0
build "" "$OUT/p2i_breakable_test" || { head -40 "$OUT/p2i_breakable_test.err"; echo "BUILD FAILED"; exit 1; }
"$OUT/p2i_breakable_test" | tee "$OUT/run.log"; rc=${PIPESTATUS[0]}
if [ -n "${GTA_IMG:-}" ]; then
  want=$(python3 -I "$REPO/tools/portable/p2i_scan_img.py" "$GTA_IMG")
  got=$(grep '^STATS ' "$OUT/run.log")
  if [ "$want" == "$got" ]; then echo "CENSUS MATCH (python == C++): $got"; else echo "CENSUS MISMATCH"; echo " python: $want"; echo " c++   : $got"; rc=1; fi
fi
[ $rc -eq 0 ] && echo "RESULT P2I OK" || echo "RESULT P2I FAIL (rc=$rc)"
if [ "${1:-}" == "mutate" ]; then
  mrc=0
  mutate() { # name, file (relative to source/game_sa), sed expr
    local D="$OUT/mut/$1"; rm -rf "$D"; mkdir -p "$D/Plugins/BreakablePlugin" "$D/FileRecords"
    cp "$S/game_sa/Plugins/BreakablePlugin/BreakablePlugin.h" "$S/game_sa/Plugins/BreakablePlugin/BreakablePlugin.cpp" "$D/Plugins/BreakablePlugin/"
    sed -i '' -e "$3" "$D/$2"
    if cmp -s "$D/$2" "$S/game_sa/$2"; then echo "MUTANT $1: sed did not change anything"; mrc=1; return; fi
    build "$D" "$D/t" || { echo "MUTANT $1: build failed"; head -5 "$D/t.err"; mrc=1; return; }
    if ( "$D/t" >"$D/run.log" 2>&1 ) 2>/dev/null; then echo "MUTANT $1: SURVIVED (test passed) <-- BAD"; mrc=1; else echo "MUTANT $1: caught ($(grep -c -E 'FAIL|runtime error|ERROR: AddressSanitizer|P2I-STUB' "$D/run.log") signals)"; fi
  }
  H=Plugins/BreakablePlugin/BreakablePlugin.h; C=Plugins/BreakablePlugin/BreakablePlugin.cpp
  mutate no-align     $H 's|static constexpr size_t kTexturesAlign = .*|static constexpr size_t kTexturesAlign = 1;|'
  mutate alloc-noalign $H 's|        size = (size + kTexturesAlign - 1) \& ~(kTexturesAlign - 1);||'
  mutate offset-noalign $H 's|sizeof(BreakInfoColor) \* m_Info.m_usNumMaterials + kTexturesAlign - 1) \& ~(kTexturesAlign - 1);|sizeof(BreakInfoColor) * m_Info.m_usNumMaterials);|'
  mutate raw-record   $C 's|#ifdef NOTSA_PORTABLE|#if 0|'
  mutate hdr-in-slot  $C 's|const uint32 sectionHeader = plugin->m_pBreakableInfo ? 1u : 0u;|const uint32 sectionHeader = (uint32)(uintptr_t)plugin->m_pBreakableInfo;|'
  mutate swap-counts  $H 's|f.numTriangles = i.m_usNumTriangles;|f.numTriangles = i.m_usNumMaterials;|'
  mutate rule-lost    $H 's|f.posRule      = (uint32)i.m_uiPosRule;||'
  mutate size-0x58    $C 's|return sizeof(BreakInfo_File);.*|return sizeof(BreakInfo_t);|'
  mutate keep-ptrs    $H 's|        i = {};||'
  [ $mrc -eq 0 ] && echo "RESULT P2I MUTANTS OK" || { echo "RESULT P2I MUTANTS FAIL"; rc=1; }
fi
exit $rc
