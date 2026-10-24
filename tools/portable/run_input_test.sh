#!/bin/bash
# MAC-W2 host test runner: tests/portable/input_test.cpp = the REAL source/portable/app_input_map.h + app_present_pacer.h (+ a real SDL3 virtual gamepad).
# `mutate`: source mutants of both headers must make the test FAIL. Needs Homebrew SDL3 (SDL3=/opt/homebrew/opt/sdl3).
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/input_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDL3="${SDL3:-/opt/homebrew/opt/sdl3}"
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
if [ -d "$SDL3/include" ]; then
  SDLINC=(-I"$SDL3/include"); SDLLIBS=(-L"$SDL3/lib" -lSDL3 -Wl,-rpath,"$SDL3/lib")
else # Linux / conan: SDL3 headers from the -isystem set of the build, SDL3 + its system libs from the game's link line
  COMPDB="${COMPDB:-$REPO/build/PortableLinux/compile_commands.json}"; BDIR="$(dirname "$COMPDB")"
  SDLINC=(-I"$(python3 - "$COMPDB" <<'PY'
import json, os, shlex, sys
for e in json.load(open(sys.argv[1])):
    c = shlex.split(e['command'])
    for i, a in enumerate(c[:-1]):
        if a == '-isystem' and os.path.exists(os.path.join(c[i + 1], 'SDL3', 'SDL.h')):
            print(c[i + 1]); sys.exit(0)
sys.exit(1)
PY
)")
  SDLLIBS=($(python3 - "$BDIR" <<'PY'
import re, sys
t = open(sys.argv[1] + '/build.ninja').read()
m = re.search(r'^build bin/gta_reversed:.*?\n  LINK_LIBRARIES = (.*?)\n', t, re.S | re.M)
print(m.group(1).replace('vendor/librw/src/librw.a', sys.argv[1] + '/vendor/librw/src/librw.a'))
PY
))
  SDK+=(-stdlib=libc++)
fi
T="$REPO/tests/portable"; S="$REPO/source/portable"
build() { # $1 = app_input_map.h, $2 = app_present_pacer.h, $3 exe  (mutants shadow the headers through a copy of the tree layout)
  local d="$OUT/tree_$(basename "$3")"; rm -rf "$d"; mkdir -p "$d/tests/portable" "$d/portable"
  cp "$T/input_test.cpp" "$d/tests/portable/"; cp "$1" "$d/portable/app_input_map.h"; cp "$2" "$d/portable/app_present_pacer.h"
  "$CLANG" "${SDK[@]}" -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w -I"$d" "${SDLINC[@]}" -o "$3" "$d/tests/portable/input_test.cpp" \
      "${SDLLIBS[@]}" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; return 1; }
}
build "$S/app_input_map.h" "$S/app_present_pacer.h" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0; ASAN_OPTIONS=detect_leaks=0:symbolize=0 SDL_VIDEODRIVER=dummy "$OUT/t" || rc=1
[ "${1:-}" = "mutate" ] || exit $rc
[ $rc = 0 ] || { echo "RESULT INPUT FAILED (positive run)"; exit 1; }
bad=0
mut() { # name, file (app_input_map.h | app_present_pacer.h), old, new
  local map="$S/app_input_map.h" pac="$S/app_present_pacer.h" src
  src="$S/$2"
  python3 - "$src" "$OUT/mut_$1.h" "$3" "$4" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read(); assert sys.argv[3] in s, 'pattern'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  [ "$2" = app_input_map.h ] && map="$OUT/mut_$1.h" || pac="$OUT/mut_$1.h"
  if ! build "$map" "$pac" "$OUT/m_$1" >/dev/null 2>&1; then echo "MUTANT $1: BUILD ERROR"; bad=$((bad+1))
  elif ASAN_OPTIONS=detect_leaks=0:symbolize=0 SDL_VIDEODRIVER=dummy "$OUT/m_$1" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed"; fi
}
mut kpdiv app_input_map.h "case SDL_SCANCODE_KP_DIVIDE:   return { KeySlot::Div };" "case SDL_SCANCODE_KP_DIVIDE:   return { KeySlot::Standard, '/' };"
mut kpperiod app_input_map.h "case SDL_SCANCODE_KP_PERIOD:   return { KeySlot::Decimal };" ""
mut kpenter app_input_map.h "case SDL_SCANCODE_KP_ENTER:    return { KeySlot::Enter };" "case SDL_SCANCODE_KP_ENTER:    return { KeySlot::ExtEnter };"
mut backspace app_input_map.h "case SDLK_BACKSPACE:    return { KeySlot::Back };" ""
mut ret app_input_map.h "case SDLK_RETURN:       return { KeySlot::ExtEnter };" "case SDLK_RETURN:       return { KeySlot::Enter };"
mut upper app_input_map.h "key - 'a' + 'A'" "key"
mut range app_input_map.h "key > 0 && key < 0x100" "key > 0"
mut fkeys app_input_map.h "key <= SDLK_F12" "key <= SDLK_F11"
mut scale app_input_map.h "(int64_t)s * kDeviceAxisMax / 32767" "(int64_t)s * kDeviceAxisMax / 32768"
mut deadzone app_input_map.h "std::abs(pos) <= 0.3f" "std::abs(pos) <= 0.0f"
mut invert app_input_map.h "pos = -pos;" ""
mut swapab app_input_map.h "j.button[1]  = p.a;" "j.button[1]  = p.b;"
mut trigger app_input_map.h "j.button[4]  = p.lt > kTriggerPressed;" "j.button[4]  = p.lt > 0;"
mut dpad app_input_map.h "j.button[13] = p.dright;" "j.button[13] = p.dleft;"
mut rstick app_input_map.h "j.lRz = ScaleAxis(p.ry);" "j.lRz = ScaleAxis(p.ly);"
mut hidden app_present_pacer.h "if (!in.windowVisible) {" "if (false) {"
mut asleep app_present_pacer.h "} else if (!in.displayAwake) {" "} else if (false) {"
mut silent app_present_pacer.h "in.nowNs - in.lastTickNs > TickFreshnessNs(period)" "false"
mut unbounded app_present_pacer.h "d.untilNs = in.nowNs + TickTimeoutNs(period);" "d.untilNs = in.nowNs + 10'000'000'000;"
mut nolimiter app_present_pacer.h "d.untilNs = in.lastPresentNs ? in.lastPresentNs + period : in.nowNs;" "d.untilNs = in.nowNs;"
mut notreq app_present_pacer.h "if (!in.vsyncRequested) {" "if (false) {"
[ $bad = 0 ] && echo "RESULT INPUT MUTATE OK" || { echo "RESULT INPUT MUTATE FAILED ($bad)"; exit 1; }
