#!/usr/bin/env python3
"""Extract the non-code image of the original gta_sa.exe (1.0 US, compact) for the standalone build.

Usage: python3 -I extract_exe_data.py [--no-initterm] <gta_sa.exe> <out_dir>

The image is the POST-`_initterm` snapshot (D1'): the exe's 1667 static initialisers are replayed with Unicorn
(tools/standalone/initterm_delta.py, needs `pip install unicorn`) and the resulting .data/.bss delta (~128 KB) is baked into the
image BEFORE the code-pointer scan (constructed objects get their vtable pointers from it). Because the delta writes into BSS, the
bin covers the whole committed range (BSS included, mostly zeros). `--no-initterm` skips the replay (pre-constructor image).

Outputs (all derived from the USER'S OWN exe, never commit them):
  original_data.bin   initial bytes of every non-code section laid out by VA (gaps = 0) + the _initterm delta; covers
                      BSS as well (zeros + delta). File offset 0 corresponds to `data_base` (see json).
  original_data.json  {image_base, data_base, data_end, committed_size, bin_size, code_lo, code_hi, sections[], stats}
  data_pointers.txt   one line per 4-byte-aligned dword in the initialised data whose value lies in [code_lo, code_hi):
                      "<addr hex> <value hex> <class> <run>"
                      class V = member of a run (>= 2) of consecutive code pointers (vtable / callback table)
                      class C = isolated code pointer (callback global, CRT hook, ...)
                      class S = isolated and looks like ASCII/UTF-16 text, NOT a pointer (ignored by the runtime)
                      run = length of the run of consecutive code-range dwords (class S is only ever assigned to run == 1)
Also prints a short summary. The PE section table is parsed, nothing is hardcoded.
"""
import json
import struct
import sys
from pathlib import Path

IMAGE_SCN_CNT_CODE = 0x20
IMAGE_SCN_MEM_EXECUTE = 0x20000000
PAGE = 0x1000
SKIP_NAMES = {".rsrc", ".reloc", ".idata_unused"}  # resources/relocs are not part of the data image


def align_up(v, a):
    return (v + a - 1) // a * a


def text_like(v):
    """dword that is really ASCII ('abc\\0') or UTF-16 ('a\\0b\\0') text, not a pointer. Keep in sync with Fixups.cpp."""
    b = [(v >> (8 * i)) & 255 for i in range(4)]
    return (b[3] == 0 and all(32 <= x < 127 for x in b[:3])) or (b[1] == 0 and b[3] == 0 and 32 <= b[0] < 127 and 32 <= b[2] < 127)


def parse_pe(d):
    if d[:2] != b"MZ":
        raise SystemExit("not an MZ/PE file")
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("bad PE signature")
    nsec, = struct.unpack_from("<H", d, pe + 6)
    opt_size, = struct.unpack_from("<H", d, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from("<H", d, opt)
    if magic != 0x10B:
        raise SystemExit("not a PE32 image")
    image_base, = struct.unpack_from("<I", d, opt + 28)
    secs = []
    o = opt + opt_size
    for i in range(nsec):
        name, vsize, va, rsize, roff, _, _, _, _, ch = struct.unpack_from("<8sIIIIIIHHI", d, o + 40 * i)
        secs.append(dict(name=name.rstrip(b"\0").decode("latin1"), rva=va, va=image_base + va, virt_size=vsize,
                         raw_size=rsize, raw_offset=roff, flags=ch,
                         code=bool(ch & (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE))))
    return image_base, secs


def main():
    args = [a for a in sys.argv[1:] if a != "--no-initterm"]
    do_initterm = "--no-initterm" not in sys.argv[1:]
    if len(args) != 2:
        raise SystemExit(__doc__)
    exe, out = Path(args[0]), Path(args[1])
    d = exe.read_bytes()
    image_base, secs = parse_pe(d)
    code = [s for s in secs if s["code"]]
    data = [s for s in secs if not s["code"] and s["name"] not in SKIP_NAMES]
    if not code or not data:
        raise SystemExit("no code or no data sections found")
    code_lo = min(s["va"] for s in code)
    code_hi = align_up(max(s["va"] + s["virt_size"] for s in code), PAGE)
    data_base = min(s["va"] for s in data)
    data_end = align_up(max(s["va"] + s["virt_size"] for s in data), PAGE)
    bin_size = data_end - data_base  # whole committed range: BSS is part of the image because the _initterm delta writes into it
    img = bytearray(bin_size)
    for s in data:
        n = min(s["raw_size"], s["virt_size"])
        if n and s["raw_offset"] + n > len(d):
            raise SystemExit(f"section {s['name']} raw data beyond end of file")
        img[s["va"] - data_base:s["va"] - data_base + n] = d[s["raw_offset"]:s["raw_offset"] + n]
    initterm = None
    if do_initterm:
        sys.path.insert(0, str(Path(__file__).resolve().parent))  # python -I does not add the script dir
        try:
            import initterm_delta
        except ImportError as e:
            raise SystemExit(f"cannot replay the exe's static initialisers: {e}\n"
                             "install unicorn into the python used by the build (pip install unicorn), or pass --no-initterm")
        delta = initterm_delta.compute(d, [(s["va"], s["raw_offset"], s["raw_size"]) for s in secs if s["name"] not in SKIP_NAMES])
        nbytes = 0
        for va, hexs in delta["ranges"]:
            b = bytes.fromhex(hexs)
            if va < data_base or va + len(b) > data_end:
                raise SystemExit(f"initterm delta range 0x{va:X}+{len(b)} outside the data image")
            img[va - data_base:va - data_base + len(b)] = b
            nbytes += len(b)
        initterm = dict(ranges=len(delta["ranges"]), bytes=nbytes, atexit_ignored=len(delta["atexit"]), rand_calls=delta["rand_calls"])
    out.mkdir(parents=True, exist_ok=True)
    (out / "original_data.bin").write_bytes(img)

    # pointer scan (4-byte aligned in VA terms); BSS is zero so only the initialised bytes matter
    assert data_base % 4 == 0
    words = struct.unpack_from(f"<{bin_size // 4}I", img, 0)
    code_ptrs, text_fp, data_ptrs = [], [], 0
    sec_of = lambda a: next((s["name"] for s in data if s["va"] <= a < s["va"] + s["virt_size"]), "?")
    for i, w in enumerate(words):
        if code_lo <= w < code_hi:
            code_ptrs.append(data_base + i * 4)
        elif data_base <= w < data_end:
            data_ptrs += 1
    # runs
    words_at = lambda a: words[(a - data_base) // 4]
    runs, i = {}, 0
    while i < len(code_ptrs):
        j = i
        while j + 1 < len(code_ptrs) and code_ptrs[j + 1] == code_ptrs[j] + 4:
            j += 1
        for k in range(i, j + 1):
            runs[code_ptrs[k]] = j - i + 1
        i = j + 1
    by_sec, by_cls = {}, {"V": 0, "C": 0}
    with open(out / "data_pointers.txt", "w") as f:
        for a in code_ptrs:
            if runs[a] >= 2:
                c = "V"
            elif text_like(words_at(a)):
                c = "S"
                text_fp.append(a)
            else:
                c = "C"
            f.write(f"0x{a:08X} 0x{words_at(a):08X} {c} {runs[a]}\n")
            if c != "S":
                by_cls[c] += 1
                by_sec[sec_of(a)] = by_sec.get(sec_of(a), 0) + 1
    stats = dict(initterm=initterm, code_pointing_dwords=len(code_ptrs) - len(text_fp), text_like_ignored=len(text_fp), data_pointing_dwords=data_ptrs,
                 code_pointing_by_section=by_sec, code_pointing_by_class=by_cls,
                 distinct_code_targets=len({words_at(a) for a in code_ptrs}))
    meta = dict(image_base=image_base, code_lo=code_lo, code_hi=code_hi, data_base=data_base, data_end=data_end,
                committed_size=data_end - data_base, bin_size=bin_size, initterm_applied=bool(initterm),
                sections=[{k: v for k, v in s.items()} for s in secs], data_sections=[s["name"] for s in data], stats=stats)
    (out / "original_data.json").write_text(json.dumps(meta, indent=2))
    print(f"data image 0x{data_base:X}..0x{data_end:X} ({data_end - data_base} B committed), bin {bin_size} B; "
          f"code range 0x{code_lo:X}..0x{code_hi:X}")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
