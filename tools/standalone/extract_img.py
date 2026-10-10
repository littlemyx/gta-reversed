#!/usr/bin/env python3
"""Extract named entries from a GTA SA IMG v2 archive (e.g. models/gta3.img): extract_img.py <img> <outdir> name1.dff name2.txd ...
Used by run_all_tests.sh (ASSETS) to get infernus.dff / male01.dff / vgsnbuild07.dff out of the game's own gta3.img."""
import struct, sys, os
img, out, names = sys.argv[1], sys.argv[2], {n.lower() for n in sys.argv[3:]}
os.makedirs(out, exist_ok=True)
with open(img, 'rb') as f:
    magic, n = struct.unpack('<4sI', f.read(8))
    assert magic == b'VER2', 'not an IMG v2 archive'
    f.seek(8)
    for _ in range(n):
        off, size_stream, size_arch, name = struct.unpack('<IHH24s', f.read(32))
        name = name.split(b'\0')[0].decode('ascii', 'replace')
        if name.lower() in names:
            pos = f.tell(); f.seek(off * 2048); data = f.read(size_stream * 2048); f.seek(pos)
            open(os.path.join(out, name), 'wb').write(data); print('extracted', name, len(data))
