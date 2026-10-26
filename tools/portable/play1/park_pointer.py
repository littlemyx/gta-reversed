#!/usr/bin/env python3
"""park_pointer.py [seconds] - keep the REAL mouse pointer at (5,5) for the first seconds of a scripted run (MAC-FPTRAP / MAC-NAN1).
The front-end menu cursor follows the real pointer: a pointer over the game window (or one a person moves while the script runs) puts the cursor on "Options"
and the scripted Returns never start a game (no "Loading the Game"). Warps once at start and then every 50 ms until the menu part of the route is over."""
import ctypes, sys, time
cg = ctypes.CDLL('/System/Library/Frameworks/ApplicationServices.framework/ApplicationServices')
class P(ctypes.Structure):
    _fields_ = [('x', ctypes.c_double), ('y', ctypes.c_double)]
cg.CGWarpMouseCursorPosition.argtypes = [P]
end = time.time() + (float(sys.argv[1]) if len(sys.argv) > 1 else 0.0)
while True:
    cg.CGWarpMouseCursorPosition(P(5, 5))
    if time.time() >= end:
        break
    time.sleep(0.05)
