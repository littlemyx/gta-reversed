#pragma once
// Non-Windows builds: `#include <Windows.h>` (direct includers outside winincl.h) resolves here when source/portable/winshim is on the
// include path (-I). It provides ONLY the scalar types and CRT names of source/portable; no Win32 API (stream W).
#include "portable/wintypes.h"
#include "portable/crt.h"
