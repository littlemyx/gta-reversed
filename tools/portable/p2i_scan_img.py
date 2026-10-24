#!/usr/bin/env python3
"""p2i_scan_img.py - independent census of the Breakable (rwID 0xFD) chunks of every DFF in an IMG v2 archive (P-2i).

Walks clump(0x10) > geometry list(0x1A) > geometry(0x0F) > extension(0x03) > chunk 0x253F2FD of each *.dff and prints ONE line the host test
(tests/portable/p2i_breakable_test.cpp) must reproduce exactly:
  STATS dffs=N chunks=N withData=N verts=N tris=N mats=N payloadBytes=N chunkBytes=N lenMismatch=N hdrValues=<n distinct non-zero>
`lenMismatch` = chunks whose length != 4 + 0x34 + 24*verts + 8*tris + 76*mats (header != 0) or != 4 (header == 0): must be 0.
Data (the game files) is never copied or committed. Usage: p2i_scan_img.py [path/to/gta3.img]   (default: the Steam copy)"""
import os, struct, sys

GTA = os.path.expanduser("~/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/"
                         "transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas")
BREAK = 0x253F2FD  # MAKECHUNKID(rwVENDORID_DEVELOPER 0x253F2, 0xFD)


def entries(img):
    assert img[:4] == b'VER2', 'not an IMG v2'
    n = struct.unpack_from('<I', img, 4)[0]
    for i in range(n):
        off, sz_a, sz_b, name = struct.unpack_from('<IHH24s', img, 8 + i * 32)
        yield name.split(b'\0')[0].decode('latin1'), off * 2048, max(sz_a, sz_b) * 2048


def walk(b, pos, end, path, out):
    while pos + 12 <= end:
        t, s, _ = struct.unpack_from('<III', b, pos)
        body = pos + 12
        if t == BREAK and path == (0x10, 0x1A, 0x0F, 0x03):
            out.append((body, s))
        elif t in (0x10, 0x1A, 0x0F, 0x03) and (not path or t != path[-1]):
            walk(b, body, min(body + s, end), path + (t,), out)
        pos = body + s


def scan(img_path):
    img = open(img_path, 'rb').read()
    st = dict(dffs=0, chunks=0, withData=0, verts=0, tris=0, mats=0, payloadBytes=0, chunkBytes=0, lenMismatch=0)
    hdrs = set()
    for name, off, sz in entries(img):
        if not name.lower().endswith('.dff'):
            continue
        b = img[off:off + sz]
        t, s, _ = struct.unpack_from('<III', b, 0)
        if t != 0x10:
            continue
        st['dffs'] += 1
        found = []
        walk(b, 12, 12 + s, (0x10,), found)
        for body, size in found:
            st['chunks'] += 1; st['chunkBytes'] += size
            hdr = struct.unpack_from('<I', b, body)[0]
            if hdr == 0:
                st['lenMismatch'] += size != 4
                continue
            hdrs.add(hdr); st['withData'] += 1
            nv, nt, nm = struct.unpack_from('<H', b, body + 4 + 4)[0], struct.unpack_from('<H', b, body + 4 + 0x14)[0], struct.unpack_from('<H', b, body + 4 + 0x20)[0]
            pay = 24 * nv + 8 * nt + 76 * nm
            st['verts'] += nv; st['tris'] += nt; st['mats'] += nm; st['payloadBytes'] += pay
            st['lenMismatch'] += size != 4 + 0x34 + pay
    return st, hdrs


if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(GTA, 'models', 'gta3.img')
    st, hdrs = scan(path)
    print('STATS ' + ' '.join('%s=%d' % kv for kv in st.items()) + ' hdrValues=%d' % len(hdrs))
