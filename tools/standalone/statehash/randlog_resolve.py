#!/usr/bin/env python3
"""Resolve the module-relative return addresses of a Win32 rand log (NOTSA_SH_RANDLOG) to function names with the linker .map.

Usage: randlog_resolve.py <randlog.txt> <gta_reversed.map> > resolved.txt
Row in : callidx frame value file:line@RVA[+WRAPRVA]       (RVA = return address of the rand hook - module base; WRAPRVA = caller of CGeneral::GetRandomNumber)
Row out: callidx frame value file:line func(@RVA) [<- wrapfunc]
The map must come from a link with /MAP (cmake -DCMAKE_EXE_LINKER_FLAGS=/MAP). Symbols are taken from the 'Publics by Value' table (Rva+Base column).
"""
import bisect
import re
import sys


def load_map(path):
    base = 0x400000
    syms = []
    rx = re.compile(r"^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8,16})\b")
    for line in open(path, encoding="latin-1"):
        m = re.search(r"Preferred load address is ([0-9A-Fa-f]+)", line)
        if m:
            base = int(m.group(1), 16)
            continue
        m = rx.match(line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return base, [s[0] for s in syms], [s[1] for s in syms]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    base, addrs, names = load_map(sys.argv[2])

    def res(rva):
        i = bisect.bisect_right(addrs, base + rva) - 1
        return names[i] if i >= 0 else "?"

    for line in open(sys.argv[1]):
        t = line.split()
        if len(t) != 4:
            print(line.rstrip())
            continue
        loc, _, rest = t[3].partition("@")
        ra, _, wrap = rest.partition("+")
        out = f"{t[0]} {t[1]} {t[2]} {loc} {res(int(ra, 16))}"
        if wrap:
            out += f" <- {res(int(wrap, 16))}"
        print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
