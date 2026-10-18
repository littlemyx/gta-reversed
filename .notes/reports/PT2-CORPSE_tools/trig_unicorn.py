import sys, struct, math
sys.path.insert(0, 'C:/Users/littl/github/gta-reversed/tools/standalone')
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
BASE, END = 0x400000, 0xcb1000
STACK, BUF = 0x2000000, 0x7000000
d = open('C:/Users/littl/github/gta-reversed/gta_sa_compact.exe', 'rb').read()
secs = [(0x401000, 0x400, 0x456400), (0x858000, 0x456800, 0x4A1C00 - 0x456800), (0x8A4000, 0x4A1C00, 0x40000)]
img = bytearray(END - BASE)
for va, raw, sz in secs:
    img[va - BASE:va - BASE + sz] = d[raw:raw + sz]
def run(fn, x):
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(BASE, END - BASE); uc.mem_write(BASE, bytes(img))
    uc.mem_map(STACK, 0x100000); uc.mem_map(BUF, 0x10000)
    uc.reg_write(UC_X86_REG_FPCW, 0x027f)
    uc.mem_write(BUF, struct.pack('<f', x))
    code_at = BUF + 0x100
    # fld dword [BUF] ; call fn ; fstp dword [BUF+4] ; hlt
    call_site = code_at + 6
    rel = (fn - (call_site + 5)) & 0xFFFFFFFF
    code = b'\xd9\x05' + struct.pack('<I', BUF) + b'\xe8' + struct.pack('<I', rel) + b'\xd9\x1d' + struct.pack('<I', BUF + 4) + b'\xf4'
    uc.mem_write(code_at, code)
    uc.reg_write(UC_X86_REG_ESP, STACK + 0x80000)
    try:
        uc.emu_start(code_at, code_at + len(code) - 1, count=100000)
    except Exception as e:
        print('exc', e, hex(uc.reg_read(UC_X86_REG_EIP)))
    return struct.unpack('<f', uc.mem_read(BUF + 4, 4))[0]
for fn in (0x821e70,):
    for x in (0.0, 0.5, 1.0, -0.5):
        print(hex(fn), x, run(fn, x), 'asin=%.6f acos=%.6f' % (math.asin(x), math.acos(x)))
