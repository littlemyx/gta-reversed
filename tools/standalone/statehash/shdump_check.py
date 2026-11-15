#!/usr/bin/env python3
"""Re-hash a statehash verbose dump (NOTSA_SH_DUMP) and compare with the per-frame hash file (NOTSA_SH_FILE).

Usage: shdump_check.py <dump.txt> <hashfile.txt>
Spec: .notes/STATEHASH_SPEC.md (FNV-1a 64 over LE uint32 words; e = exact float bits, q = float quantised to 1/256; pool = Mix of per-entity Accs).
Exit code 0 = every dumped subsystem (and `all` when all nine are present) matches; 1 = mismatch; 2 = usage / parse error.
"""
import math
import struct
import sys

OFF = 0xCBF29CE484222325
PRIME = 0x100000001B3
M64 = (1 << 64) - 1
SUBS = ["tm", "ck", "rng", "pop", "pl", "cam", "pd", "vh", "ob"]
POOLS = ("pd", "vh", "ob")


def feed(h, v):
    for i in range(4):
        h ^= (v >> (8 * i)) & 0xFF
        h = (h * PRIME) & M64
    return h


class Acc:
    def __init__(self):
        self.e = OFF
        self.q = OFF

    def u(self, v):
        v &= 0xFFFFFFFF
        self.e = feed(self.e, v)
        self.q = feed(self.q, v)

    def f(self, bits):
        f = struct.unpack("<f", struct.pack("<I", bits))[0]
        self.e = feed(self.e, bits)
        if math.isfinite(f) and abs(f) < 8e6:
            x = f * 256.0  # exact
            r = math.floor(abs(x) + 0.5)  # lround: half away from zero
            self.q = feed(self.q, (-r if x < 0 else r) & 0xFFFFFFFF)
        else:
            self.q = feed(self.q, 0x7FFFFFFF)

    def mix(self, o):
        self.e = feed(feed(self.e, o.e & 0xFFFFFFFF), o.e >> 32)
        self.q = feed(feed(self.q, o.q & 0xFFFFFFFF), o.q >> 32)


def word(acc, val):
    if val.startswith("u:"):
        acc.u(int(val[2:], 16))
    elif val.startswith("f:"):
        acc.f(int(val.split(":")[1], 16))
    else:
        raise ValueError("bad value " + val)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    hashes = {}
    for line in open(sys.argv[2]):
        if line.startswith("#") or not line.strip():
            continue
        t = line.split()
        hashes[int(t[0])] = (int(t[1]), dict(zip(SUBS + ["all"], (int(x, 16) for x in t[2:12]))))
    # frame -> sub -> list of (key, field, val)
    rows = {}
    for line in open(sys.argv[1]):
        t = line.split()
        if len(t) != 5:
            continue
        rows.setdefault(int(t[0]), {}).setdefault(t[1], []).append((t[2], t[3], t[4]))
    bad = 0
    checked = 0
    for frame in sorted(rows):
        if frame not in hashes:
            print(f"frame {frame}: not in hash file")
            bad += 1
            continue
        accs = {}
        for sub, rs in rows[frame].items():
            if sub in POOLS:
                total = Acc()
                cur, curkey = None, None
                for key, _field, val in rs:
                    if key != curkey:
                        if cur is not None:
                            total.mix(cur)
                        cur, curkey = Acc(), key
                    word(cur, val)
                if cur is not None:
                    total.mix(cur)
                accs[sub] = total
            else:
                a = Acc()
                for _key, _field, val in rs:
                    word(a, val)
                accs[sub] = a
        for sub, a in accs.items():
            checked += 1
            want = hashes[frame][1][sub]
            if a.e != want:
                print(f"frame {frame} {sub}: dump hashes to {a.e:016x}, file says {want:016x}  MISMATCH")
                bad += 1
            else:
                print(f"frame {frame} {sub}: {a.e:016x} ok ({len(rows[frame][sub])} rows)")
        # `all` is checkable when every part was dumped; pl / pools with no rows are the empty Acc
        if sub_complete(rows[frame]):
            parts = [accs.get(s, Acc()) for s in ("tm", "ck", "rng", "pop", "pl", "cam", "pd", "vh", "ob")]
            al = Acc()
            for p in parts:
                al.mix(p)
            checked += 1
            want = hashes[frame][1]["all"]
            if al.e != want:
                print(f"frame {frame} all: dump hashes to {al.e:016x}, file says {want:016x}  MISMATCH")
                bad += 1
            else:
                print(f"frame {frame} all: {al.e:016x} ok")
    print(f"checked {checked} hashes, {bad} mismatches")
    return 1 if bad else 0


def sub_complete(r):
    # tm ck rng pop cam must have rows; pl / pd / vh / ob may be empty (no player / empty pool), so `all` is only trusted when the dump covered all nine
    # subsystems requested: we cannot tell "requested but empty" from "not requested" -> require the five always-nonempty ones plus at least pd
    return all(s in r for s in ("tm", "ck", "rng", "pop", "cam", "pd", "vh"))


if __name__ == "__main__":
    sys.exit(main())
