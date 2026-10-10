#!/usr/bin/env python3
"""obj_code_sig.py - fingerprint the CODE of COFF .obj files (regular and /bigobj) so two builds can be compared for "bit-identical object code".

usage:  python3 -I tools/standalone/obj_code_sig.py sig  <out.json> <obj>...            fingerprint objects (per object: code-section hashes)
        python3 -I tools/standalone/obj_code_sig.py cmp  <a.json> <b.json> [name-regex]  compare two fingerprints, print the objects that differ

A fingerprint of an object = sorted list over its code sections (IMAGE_SCN_CNT_CODE) of
  (section name, sha1(raw bytes), sha1(relocations as (offset - section-relative, target symbol NAME, type)))
Section raw bytes are compared as is (rel32 call/jmp displacements are zero in the object, the relocation list carries the targets by name), so
two objects are "bit-identical code" iff the lists are equal. Debug info ($S/$T), data sections and the COMDAT order of the object are NOT part of it
(comdat sections are sorted by name). With `data` as an extra arg the writable/readonly data sections (.data/.bss/.rdata/.xdata is skipped) are hashed too.
"""
import hashlib
import json
import struct
import sys

IMAGE_SCN_CNT_CODE = 0x20
IMAGE_SCN_CNT_INIT = 0x40


def parse(path, with_data=False):
    d = open(path, "rb").read()
    if d[0:4] == b"\x00\x00\xff\xff" and d[4:6] == b"\x00\x00" or d[0:2] == b"\x00\x00" and d[2:4] == b"\xff\xff":
        # bigobj: sig1 0, sig2 0xFFFF, version, machine, timestamp, ... (ANON_OBJECT_HEADER_BIGOBJ)
        nsec, symptr, nsym = struct.unpack_from("<I", d, 44)[0], struct.unpack_from("<I", d, 48)[0], struct.unpack_from("<I", d, 52)[0]
        hdr, secsz, symsz = 56, 40, 20
        symrec = 20
    else:
        machine, nsec, ts, symptr, nsym, optsz, ch = struct.unpack_from("<HHIIIHH", d, 0)
        hdr = 20 + optsz
        secsz, symsz = 40, 18
        symrec = 18
    strtab = symptr + nsym * symrec

    def sname(raw):
        s = raw.rstrip(b"\0").decode("latin1")
        if s.startswith("/") and s[1:].isdigit():
            off = strtab + int(s[1:])
            e = d.index(b"\0", off)
            return d[off:e].decode("latin1")
        return s

    # symbols
    names = []
    i = 0
    while i < nsym:
        o = symptr + i * symrec
        nm = d[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", nm, 4)[0]
            p = strtab + off
            n = d[p:d.index(b"\0", p)].decode("latin1")
        else:
            n = nm.rstrip(b"\0").decode("latin1")
        aux = d[o + (19 if symrec == 20 else 17)]
        names.append(n)
        for _ in range(aux):
            names.append("")
        i += 1 + aux
    sig = []
    for s in range(nsec):
        o = hdr + s * secsz
        name = sname(d[o:o + 8])
        vsz, va, rawsz, rawptr, relptr, lnptr, nrel, nln, flags = struct.unpack_from("<IIIIIIHHI", d, o + 8)
        is_code = bool(flags & IMAGE_SCN_CNT_CODE)
        if not is_code and not (with_data and (name.startswith((".data", ".bss", ".rdata")))):
            continue
        raw = d[rawptr:rawptr + rawsz] if rawptr else b""
        rel = []
        for r in range(nrel):
            ro = relptr + r * 10
            vaddr, symidx, typ = struct.unpack_from("<IIH", d, ro)
            rel.append((vaddr, names[symidx] if symidx < len(names) else "?", typ))
        sig.append((name, hashlib.sha1(raw).hexdigest(), hashlib.sha1(repr(rel).encode()).hexdigest(), len(raw)))
    sig.sort()
    return sig


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    if sys.argv[1] == "sig":
        args = sys.argv[3:]
        data = False
        if args and args[0] == "--data":
            data = True
            args = args[1:]
        out = {}
        for p in args:
            try:
                out[p] = parse(p, data)
            except Exception as e:
                out[p] = "ERR %s" % e
        json.dump(out, open(sys.argv[2], "w"))
        print("fingerprinted", len(out))
        return 0
    if sys.argv[1] == "cmp":
        import re
        a, b = json.load(open(sys.argv[2])), json.load(open(sys.argv[3]))
        rx = re.compile(sys.argv[4]) if len(sys.argv) > 4 else None
        diff = same = 0
        for k in sorted(set(a) & set(b)):
            if rx and not rx.search(k):
                continue
            if a[k] == b[k]:
                same += 1
            else:
                diff += 1
                print("DIFF", k)
        print("same %d, different %d, only-in-a %d, only-in-b %d" % (same, diff, len(set(a) - set(b)), len(set(b) - set(a))))
        return 1 if diff else 0


sys.exit(main())
