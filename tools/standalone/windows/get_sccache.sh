#!/bin/bash
# Install sccache (the opt-in MSVC compile cache, docs/WINDOWS_SETUP.md "Compile cache") as a USER-LOCAL binary: C:/gta-tools/sccache/sccache.exe.
# Downloads ONLY the official GitHub release asset of mozilla/sccache and verifies the sha256 published next to it; no admin, no system settings.
# usage: tools/standalone/windows/get_sccache.sh [version]      (default v0.18.0)   env: GTA_TOOLS (default C:/gta-tools)
set -eu
VER="${1:-v0.18.0}"; TOOLS="${GTA_TOOLS:-C:/gta-tools}"; DEST="$TOOLS/sccache"
if [ -x "$DEST/sccache.exe" ] && "$DEST/sccache.exe" --version 2>/dev/null | grep -q "${VER#v}"; then echo "sccache $VER already in $DEST"; exit 0; fi
A="sccache-$VER-x86_64-pc-windows-msvc.zip"; URL="https://github.com/mozilla/sccache/releases/download/$VER/$A"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/sccache-dl.XXXXXX")"; trap 'rm -rf "$TMP"' EXIT
curl -fsSL -o "$TMP/$A" "$URL"; curl -fsSL -o "$TMP/$A.sha256" "$URL.sha256"
want="$(tr -d ' \r\n' < "$TMP/$A.sha256" | cut -c1-64)"; got="$(sha256sum "$TMP/$A" | cut -c1-64)"
[ "$want" = "$got" ] || { echo "sccache: sha256 mismatch (want $want, got $got)" >&2; exit 1; }
mkdir -p "$TMP/x" "$DEST"; unzip -q -o "$TMP/$A" -d "$TMP/x"
cp "$TMP/x/"*/sccache.exe "$DEST/sccache.exe"
echo "sccache $VER sha256 $got -> $DEST/sccache.exe"; "$DEST/sccache.exe" --version
