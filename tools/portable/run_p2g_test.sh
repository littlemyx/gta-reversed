#!/bin/bash
# P-2g host test (not wired in CMake; MAC-W0 owns CMake): tests/portable/p2g_script_test.cpp
#  - goldens (CRunningScript_File 0xE0: exe / port / mission) come from tools/portable/gen_x86_golden.py (independent of the code under test)
#  - REAL FileRecords/ScriptFile.hpp / FileRecord.hpp / GenericGameStorage.h / ScriptParam.h, the `class CRunningScript` data members and the FileRecord<CRunningScript> trait cut verbatim from
#    Scripts/RunningScript.h, and the per-script loops of CTheScripts::Save / CTheScripts::Load extracted verbatim from Scripts/TheScripts.cpp
#  - real main.scm: P2G_MAIN_SCM (default: the Steam copy, never committed); `mutate` builds mutants of the generated copies and requires every one to FAIL the test
# Compile flags (-D set, std, arch) come from build/PortableMac/compile_commands.json.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2g_test}"; mkdir -p "$OUT"
export P2G_MAIN_SCM="${P2G_MAIN_SCM:-$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas/data/script/main.scm}"
S="$REPO/source"
# CI (GitHub sets CI=true) has no game data: the real-main.scm part is skipped there instead of failing
[ -r "$P2G_MAIN_SCM" ] || [ -z "${CI:-}" ] || export P2G_ALLOW_NO_SCM=1
{ echo '#include <cstdint>'; python3 "$REPO/tools/portable/gen_x86_golden.py" cpp CRunningScript_File; } > "$OUT/p2g_golden.inc" || exit 1
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
open(out + '/inc/FileRecords/ScriptFile.hpp', 'w').write(open(repo + '/source/game_sa/FileRecords/ScriptFile.hpp').read())
h = open(repo + '/source/game_sa/Scripts/RunningScript.h').read()
th = open(repo + '/source/game_sa/Scripts/TheScripts.h').read()
main = re.search(r"MAIN_SCRIPT_SIZE\s*=\s*([\d']+);", th).group(1); mis = re.search(r"MISSION_SCRIPT_SIZE\s*=\s*([\d']+);", th).group(1)
enum = re.search(r"enum \{\n    MAX_STACK_DEPTH.*?\n\};", h, re.S).group(0)
ssz = re.search(r"constexpr auto SHORT_STRING_SIZE\s*=\s*\d+;", h).group(0)
sstr = re.search(r"using ShortString = char\[SHORT_STRING_SIZE\];", h).group(0)
a = h.index('class CRunningScript {'); b = h.index('m_ThisMustBeTheOnlyMissionRunning;', a); b = h.index('\n', b)
setip = re.search(r'    void SetCurrentIp\(uint8\* ip\)[^\n]*\n', h).group(0)
cls = h[a:b + 1] + setip + '};\n'
open(out + '/p2g_cut.h', 'w').write('#pragma once\n#include "ScriptParam.h"\nconstexpr uint32 MAIN_SCRIPT_SIZE = %s;\nconstexpr uint32 MISSION_SCRIPT_SIZE = %s;\n%s\n%s\nnamespace scm { %s }\n%s' % (main, mis, enum, ssz, sstr, cls))
t = h.index('template<> struct notsa::FileRecord<CRunningScript> {')
open(out + '/p2g_trait.inc', 'w').write(h[t:])
c = open(repo + '/source/game_sa/Scripts/TheScripts.cpp').read()
sa = c.index('    for (auto* s = lastScript; s; s = s->m_pPrev) {'); sb = c.index('\n}\n', sa)
la = c.index('    auto numScripts = CGenericGameStorage::LoadDataFromWorkBuffer<uint32>();'); lb = c.index('\n}\n', la)
open(out + '/p2g_snippets.inc', 'w').write('void CTheScripts::SaveLoop(CRunningScript* lastScript) {\n%s\n}\nvoid CTheScripts::LoadLoop() {\n%s\n}\n' % (c[sa:sb], c[la:lb]))
PY
}
build() {  # $1 = work dir, $2 = exe
eval "nice -n 10 $BASE -I'$1/inc' -I'$REPO/tests/portable/p2c_stubs' -I$S/game_sa/Scripts -I$S/game_sa/Core -I$S/game_sa -I$S/game_sa/Enums -I$S -I'$1' -I'$OUT' -w -fsanitize=address,undefined -g -DREPO_DIR=$REPO -o '$2' '$REPO/tests/portable/p2g_script_test.cpp'" 2>"$1/build.err" || { head -40 "$1/build.err"; return 1; }
}
gen "$OUT" || exit 1
build "$OUT" "$OUT/p2g_script_test" || { echo "BUILD FAILED"; exit 1; }
"$OUT/p2g_script_test"; rc=$?
[ "${1:-}" = "mutate" ] || exit $rc
[ $rc -eq 0 ] || exit $rc
# ---- mutants: each edit of the generated copy must make the test fail ----------------------------------------------------------------------------------------------------------
MUT=(
 'p2g_trait.inc|f.next = 0;|f.next = 1;'
 'p2g_trait.inc|f.prev = 0;|f.prev = 1;'
 'p2g_trait.inc|std::memcpy(f.name, s.m_szName, sizeof(f.name));|(void)0;'
 'p2g_trait.inc|f.baseIP = notsa::script_file::ToExeVA(s.m_BaseIP, space, size);|f.baseIP = 0;'
 'p2g_trait.inc|f.ip     = notsa::script_file::ToExeVA(s.m_IP, space, size);|f.ip = 0;'
 'p2g_trait.inc|f.ipStack[i] = notsa::script_file::ToExeVA(s.m_IPStack[i], space, size);|f.ipStack[i] = 0;'
 'p2g_trait.inc|f.stackDepth = s.m_StackDepth;|f.stackDepth = 0;'
 'p2g_trait.inc|f.localVars[i] = s.m_LocalVars[i].uParam;|f.localVars[i] = s.m_LocalVars[i ? i - 1 : 0].uParam;'
 'p2g_trait.inc|f.usesMissionCleanup             = s.m_UsesMissionCleanup ? 1 : 0;|f.usesMissionCleanup = s.m_IsExternal ? 1 : 0;'
 'p2g_trait.inc|f.externalType                   = s.m_ExternalType;|f.externalType = 0;'
 'p2g_trait.inc|f.wakeTime                       = s.m_WakeTime;|f.wakeTime = 0;'
 'p2g_trait.inc|f.andOrState                     = s.m_AndOrState;|f.andOrState = 0;'
 'p2g_trait.inc|f.notFlag                        = s.m_NotFlag ? 1 : 0;|f.notFlag = 0;'
 'p2g_trait.inc|f.sceneSkipIP                    = s.m_SceneSkipIP;|f.sceneSkipIP = 0;'
 'p2g_trait.inc|f.thisMustBeTheOnlyMissionRunning = s.m_ThisMustBeTheOnlyMissionRunning ? 1 : 0;|f.thisMustBeTheOnlyMissionRunning = 0;'
 'p2g_trait.inc|f.isDeathArrestCheckEnabled      = s.m_IsDeathArrestCheckEnabled ? 1 : 0;|f.isDeathArrestCheckEnabled = s.m_DoneDeathArrest ? 1 : 0;'
 'p2g_trait.inc|s.m_IP = nullptr;|(void)0;'
 'p2g_trait.inc|s.m_IPStack.fill(nullptr);|(void)0;'
 'p2g_trait.inc|s.m_StackDepth = f.stackDepth;|(void)0;'
 'p2g_trait.inc|s.m_LocalVars[i].uParam = f.localVars[i];|s.m_LocalVars[i].uParam = f.localVars[(i + 1) % 34];'
 'p2g_trait.inc|s.m_WakeTime                  = f.wakeTime;|(void)0;'
 'p2g_trait.inc|s.m_SceneSkipIP               = f.sceneSkipIP;|(void)0;'
 'p2g_trait.inc|s.m_DoneDeathArrest           = f.doneDeathArrest != 0;|s.m_DoneDeathArrest = f.doneDeathArrest == 1;'
 'p2g_trait.inc|(s.m_ThisMustBeTheOnlyMissionRunning ? CTheScripts::MissionBlock.data() : CTheScripts::ScriptSpace.data())|(s.m_ThisMustBeTheOnlyMissionRunning ? CTheScripts::ScriptSpace.data() : CTheScripts::MissionBlock.data())'
 'p2g_trait.inc|s.m_BaseIP = base ? base :|s.m_BaseIP = false ? base :'
 'p2g_trait.inc|std::memcpy(s.m_szName, f.name, sizeof(f.name));|(void)0;'
 'p2g_trait.inc|CRunningScript& s, const Type& f) {|CRunningScript\& s, const Type\& f) { s.m_pNext = nullptr;'
 'inc/FileRecords/ScriptFile.hpp|return (p \&\& d < size)|return (p \&\& d <= size)'
 'inc/FileRecords/ScriptFile.hpp|va - ScriptSpaceExeVA < size)|va - ScriptSpaceExeVA <= size)'
 'inc/FileRecords/ScriptFile.hpp|va >= ScriptSpaceExeVA \&\&|va > ScriptSpaceExeVA \&\&'
 'inc/FileRecords/ScriptFile.hpp|ScriptSpaceExeVA + (uint32_t)d : 0u|ScriptSpaceExeVA + (uint32_t)d + 1 : 0u'
 'p2g_snippets.inc|stk = \&ScriptSpace[ip];|stk = \&ScriptSpace[ip + 1];'
 'p2g_snippets.inc|script->SetCurrentIp(\&ScriptSpace[CGenericGameStorage::LoadDataFromWorkBuffer<uint32>()]);|script->m_IP = nullptr; CGenericGameStorage::LoadDataFromWorkBuffer<uint32>();'
 'p2g_snippets.inc|script->m_pPrev = prev;|script->m_pPrev = nullptr;'
 'p2g_snippets.inc|CGenericGameStorage::SaveDataToWorkBuffer(*s);|CGenericGameStorage::SaveDataToWorkBuffer(s->m_szName, 8);'
 'p2g_snippets.inc|(uint32)(stk ? stk - ScriptSpace.data() : 0)|(uint32)(stk ? stk - ScriptSpace.data() + 1 : 0)'
)
n=0; caught=0
for m in "${MUT[@]}"; do
  IFS='|' read -r f old new <<<"$m"; n=$((n+1)); W="$OUT/mut"; rm -rf "$W"; mkdir -p "$W"; gen "$W"
  python3 - "$W/$f" "$old" "$new" <<'PY' || { echo "mutant $n: pattern not found: $old"; continue; }
import sys
p, old, new = sys.argv[1:4]
old = old.replace('\\', ''); new = new.replace('\\', '')
s = open(p).read(); assert old in s, old
open(p, 'w').write(s.replace(old, new, 1))
PY
  if build "$W" "$W/t" >/dev/null 2>&1; then if "$W/t" >/dev/null 2>&1; then echo "mutant $n SURVIVED: $f: $old"; else caught=$((caught+1)); fi; else caught=$((caught+1)); fi   # build break counts as caught
done
echo "mutants caught: $caught/$n"; [ $caught -eq $n ]
