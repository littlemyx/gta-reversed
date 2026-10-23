#!/usr/bin/env python3
"""p2h_scan_nodes.py - independent census of the 64 data/paths/NODES%d.DAT files (P-2h).

Decodes every file from raw bytes with Python's `struct` (no game code) and prints ONE line that the host test (tests/portable/p2h_pathnode_test.cpp) must reproduce exactly from the
nodes it decoded through the real CPathNode_File / FileRecord<CPathNode> / LoadPathNodesFromFile (every field and every bit-field member summed separately).
File layout (PathFind.cpp LoadPathFindData): 5 x u32 header (numNodes, numVehicleNodes, numPedNodes, numCarPathLinks, numAddresses), numNodes x 28 node, numCarPathLinks x 14 CCarPathLink,
then (numToAdd = numAddresses + 16*12 = numAddresses + 192) CNodeAddress[numToAdd] (4 B), CCarPathLinkAddress[numAddresses] (2 B), u8 linkLengths[numToAdd], CPathIntersectionInfo[numToAdd] (1 B):
   file_size == 20 + 28*n + 14*links + 4*(a+192) + 2*a + (a+192) + (a+192) == 20 + 28*n + 14*links + 8*a + 1152
Data (the game files) is never copied or committed. Usage: p2h_scan_nodes.py [dir with NODES*.DAT]   (default: the Steam copy)"""
import os, struct, sys

GTA = os.path.expanduser("~/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/"
                         "transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas")
EXTRA = 16 * 12  # NUM_DYNAMIC_LINKS_PER_AREA * 12


def scan(d):
    st = {k: 0 for k in ('files', 'nodes', 'veh', 'ped', 'carLinks', 'addrs', 'bytes', 'countBad', 'sizeBad', 'nextNonZero', 'prevNonZero', 'byte3NonZero', 'areaBad', 'nodeBad',
                          'sumX', 'sumY', 'sumZ', 'sumDist', 'sumBase', 'sumWidth', 'sumFlood', 'flood0', 'maxNumLinks', 'sumNumLinks', 'sumSpawn', 'sumBehaviour',
                          'linkRangeBad', 'linkAreaBad', 'linkNodeBad', 'linkInvalid', 'emptyAreas')}
    bits = [0] * 16
    for k in range(64):
        b = open(os.path.join(d, 'NODES%d.DAT' % k), 'rb').read()
        n, veh, ped, links, addrs = struct.unpack_from('<5I', b, 0)
        st['files'] += 1; st['nodes'] += n; st['veh'] += veh; st['ped'] += ped; st['carLinks'] += links; st['addrs'] += addrs; st['bytes'] += len(b)
        st['countBad'] += n != veh + ped
        st['sizeBad'] += len(b) != 20 + 28 * n + 14 * links + 8 * addrs + 6 * EXTRA
        st['emptyAreas'] += n == 0
    # second pass: node decode + cross references (needs every area's node count)
    counts = []
    for k in range(64):
        b = open(os.path.join(d, 'NODES%d.DAT' % k), 'rb').read()
        counts.append(struct.unpack_from('<I', b, 0)[0])
    for k in range(64):
        b = open(os.path.join(d, 'NODES%d.DAT' % k), 'rb').read()
        n, veh, ped, links, addrs = struct.unpack_from('<5I', b, 0)
        nl_off = 20 + 28 * n + 14 * links
        for i in range(n):
            o = 20 + 28 * i
            l0, l1, x, y, z, dist, base, area, node, width, flood, w = struct.unpack_from('<IIhhhhhHHBBI', b, o)
            st['nextNonZero'] += l0 != 0; st['prevNonZero'] += l1 != 0
            st['byte3NonZero'] += (w >> 24) != 0
            st['areaBad'] += area != k; st['nodeBad'] += node != i
            st['sumX'] += x; st['sumY'] += y; st['sumZ'] += z; st['sumDist'] += dist; st['sumBase'] += base; st['sumWidth'] += width; st['sumFlood'] += flood
            st['flood0'] += flood == 0
            nlinks = w & 0xF
            st['maxNumLinks'] = max(st['maxNumLinks'], nlinks); st['sumNumLinks'] += nlinks
            st['sumSpawn'] += (w >> 16) & 0xF; st['sumBehaviour'] += (w >> 20) & 0xF
            for j in range(4, 16):
                bits[j] += (w >> j) & 1
            # link range / targets
            if base < 0 or base + nlinks > addrs + EXTRA:
                st['linkRangeBad'] += 1
            else:
                for j in range(nlinks):
                    ta, tn = struct.unpack_from('<HH', b, nl_off + 4 * (base + j))
                    if ta == 0xFFFF or tn == 0xFFFF:
                        st['linkInvalid'] += 1
                    elif ta >= 64:
                        st['linkAreaBad'] += 1
                    elif tn >= counts[ta]:
                        st['linkNodeBad'] += 1
    return st, bits


if __name__ == '__main__':
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(GTA, 'data', 'paths')
    st, bits = scan(d)
    print('STATS ' + ' '.join('%s=%d' % kv for kv in st.items()) + ' bits4_15=' + ','.join(map(str, bits[4:])))
