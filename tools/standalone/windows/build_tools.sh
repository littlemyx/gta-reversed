#!/bin/bash
# builds the WIN-PERF native tools into $1 (default C:/gta-perf/px): d3d9.dll (Present-log proxy), inputdrv.exe, present_probe.exe. Needs the MSVC x86 env
# (source tools/standalone/windows/msvcenv.sh).
D="$(cd "$(dirname "$0")" && pwd)"; OUT="${1:-C:/gta-perf/px}"; mkdir -p "$OUT"
bash "$D/d3d9px/build.sh" "$OUT" >/dev/null || exit 1
cd "$OUT" || exit 1
cl //nologo //O2 //Fo:inputdrv.obj //Fe:inputdrv.exe "$(cygpath -w "$D/inputdrv.c")" user32.lib >/dev/null || exit 1
cl //nologo //O2 //Fo:present_probe.obj //Fe:present_probe.exe "$(cygpath -w "$D/present_probe.c")" user32.lib d3d9.lib >/dev/null || exit 1
echo "built into $OUT"
