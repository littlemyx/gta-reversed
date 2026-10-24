#!/bin/bash
# MAC-W3 host test runner: tests/portable/dsound_al_test.cpp = the REAL source/portable/dsound_al.cpp (DirectSound over a software mixer + OpenAL) with its output captured from an
# ALC_SOFT_loopback device (needs Homebrew openal-soft; no sound card, nothing audible). `mutate`: broken mixers must FAIL the test.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/dsound_al_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
AL="${OPENAL_PREFIX:-/opt/homebrew/opt/openal-soft}"
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
LIBCXX=(); [ -d /opt/homebrew/opt/llvm/lib/c++ ] && LIBCXX=(-stdlib=libc++ -L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++)
T="$REPO/tests/portable"; S="$REPO/source/portable"
build() { # $1 = dsound_al.cpp to test (mutants shadow a copy of the source tree layout), $2 exe
  local d="$OUT/tree_$(basename "$2")"; rm -rf "$d"; mkdir -p "$d/tests/portable" "$d/source"
  cp "$T/dsound_al_test.cpp" "$d/tests/portable/"; cp -R "$S" "$d/source/portable"; cp "$1" "$d/source/portable/dsound_al.cpp"
  "$CLANG" "${SDK[@]}" "${LIBCXX[@]}" -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w -DNOTSA_PORTABLE -I"$d/source" -I"$AL/include" \
      -o "$2" "$d/tests/portable/dsound_al_test.cpp" "$d/source/portable/dsound_al.cpp" -L"$AL/lib" -lopenal -Wl,-rpath,"$AL/lib" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; return 1; }
}
build "$S/dsound_al.cpp" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0; ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t" "$OUT/capture.wav" 2>"$OUT/run.err" || rc=1
[ "${1:-}" = "mutate" ] || { [ $rc = 0 ] || { ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t" "$OUT/capture.wav" 2>&1 | grep -E "FAIL|^RESULT|checks," | head -30; }; exit $rc; }
[ $rc = 0 ] || { echo "RESULT DSOUND_AL FAILED (positive run)"; exit 1; }
bad=0
mut() { # name, old, new
  python3 - "$S/dsound_al.cpp" "$OUT/mut_$1.cpp" "$2" "$3" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read(); assert sys.argv[3] in s, 'pattern'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  if ! build "$OUT/mut_$1.cpp" "$OUT/m_$1" >/dev/null 2>&1; then echo "MUTANT $1: BUILD ERROR"; bad=$((bad+1))
  elif ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/m_$1" "$OUT/m_capture.wav" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed"; fi
}
mut volume "(double)volume / 2000.0" "(double)volume / 1000.0"
mut panside "if (pan > 0) {" "if (pan < 0) {"
mut freqratio "(double)(freq ? freq : fmt.nSamplesPerSec) / (double)outRate" "(double)outRate / (double)(freq ? freq : fmt.nSamplesPerSec)"
mut noloop "            if (looping) {
                pos = std::fmod(pos, (double)nfr);" "            if (false) {
                pos = std::fmod(pos, (double)nfr);"
mut noclip "std::clamp(std::lrintf(v), -32768L, 32767L)" "(long)(std::int16_t)std::lrintf(v)"
mut lockwrap "if (p2) { *p2 = first < bytes ? s->data.data() : nullptr; }" "if (p2) { *p2 = nullptr; }"
mut rightaxis "Vec3 right = Cross(L.top, L.front);" "Vec3 right = Cross(L.front, L.top);"
mut deferred "if (apply == DS3D_IMMEDIATE) { f(s->cur); } else { s->dirty = true; }" "f(s->cur);"
mut writecursor "return (PlayCursor() + lead) % (DWORD)data.size();" "return PlayCursor();"
mut endcursor "                pos     = 0.0;
                playing = false;
                break;" "                playing = false;
                break;"
mut setpos "s->pos = (double)(p / s->Block());" "s->pos = 0.0;"
mut sumbuf "acc[2 * i]     += l * cL;" "acc[2 * i]     = l * cL;"
mut rolloff "att = cur.minDist / (cur.minDist + L.rolloffFactor * (d - cur.minDist));" "att = 1.f;"
mut stopkeep "        s->playing = false;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock" "        s->playing = false; s->pos = 0.0;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock"
mut nopan3d "gL *= att;
        gR *= att;" "gL *= att;
        gR *= att; gL = gR = std::max(gL, gR);"
[ $bad = 0 ] && echo "RESULT DSOUND_AL MUTATE OK" || { echo "RESULT DSOUND_AL MUTATE FAILED ($bad)"; exit 1; }
