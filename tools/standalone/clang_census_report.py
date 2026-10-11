#!/usr/bin/env python3
"""Aggregate the logs of clang_census.py into counts per category / file (markdown tables on stdout).

  clang_census_report.py --out DIR --mode msvc|native [--top 20] [--tsv DIR/classified.tsv]

A diagnostic is counted twice: raw (per TU, every re-parse of a header counts again) and unique ((file, line, message)
= one root cause in the source tree).  The tables of .notes/STREAM_C_PLAN.md use the UNIQUE numbers unless stated.
"""
import argparse, collections, os, re, sys

DIAG = re.compile(r'^(?P<file>/[^:(]+)[:(](?P<line>\d+)[:,](?P<col>\d+)\)?: (?P<sev>error|fatal error): (?P<msg>.*)$')
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')) + '/'

WIN_NAMES = re.compile(
    r"\b(D3D\w*|IDirect\w+|LPDIRECT\w+|DSBUFFER\w*|DSBCAPS|DS3D\w*|DSERR\w*|DSSCL\w*|DIERR\w*|DIDEVICE\w*|DIJOYSTATE\w*|DIK_\w+|DI\w*[A-Z]\w*|WAVEFORMAT\w*|WAVE_\w+|MMSYSERR\w*|"
    r"HWND|HINSTANCE|HDC|HGLRC|HMENU|HICON|HBITMAP|HFONT|HPEN|HBRUSH|HMONITOR|HRESULT|HRGN|LPARAM|WPARAM|LRESULT|LPCTSTR|LPTSTR|TCHAR|MSG|WNDCLASS\w*|RECT|POINT|SIZE|RGNDATA|DEVMODE\w*|"
    r"CRITICAL_SECTION|SECURITY_ATTRIBUTES|OVERLAPPED|FILETIME|SYSTEMTIME|WIN32_FIND_DATA\w*|LARGE_INTEGER|MEMORYSTATUS\w*|OSVERSIONINFO\w*|SYSTEM_INFO|"
    r"WM_\w+|VK_\w+|SW_\w+|MB_\w+|IDOK|IDCANCEL|IDYES|IDNO|GWL_\w+|WS_\w+|CS_\w+|SM_\w+|SPI_\w+|SWP_\w+|HWND_\w+|PM_\w+|CW_\w+|"
    r"(Create|Open|Close|Read|Write|Delete|Set|Get|Find|Wait|Release|Reset|Suspend|Resume|Terminate|Exit|Enter|Leave|Initialize|Query|Load|Free|Show|Update|Register|Unregister|Dispatch|Translate|Peek|Post|Send|Adjust|Clip|Enum|Flush|Lock|Unlock|Map|Unmap|Duplicate|Interlocked|Heap|Virtual|Local|Global|Co|Ole|Dde|Output|Message|Is)\w*(Thread|Mutex|Event|Semaphore|File|Handle|Process|Module|Window|Cursor|Capture|Timer|Time|Tick|Counter|Performance|Sleep|Object|Objects|Section|Path|Directory|Library|ProcAddress|Message|Class|Memory|Heap|Debugger|Device|Mode|Rect|Menu|Placement|Style|Long|Ptr|Info|Metrics|Instance|Interface|Exchange|Increment|Decrement|Compare|Beep|Error|String|Resource|Registry|Key|Value|Clipboard|Foreground|Focus|Active|Visible|Zoomed|Iconic)\w*|"
    r"Sleep|GetLastError|SetLastError|GetTickCount\w*|timeGetTime|timeBeginPeriod|timeEndPeriod|QueryPerformance\w+|DefWindowProc\w*|WinMain|wWinMain|MessageBox\w*|ShowCursor|ClipCursor|SetCursor\w*|LoadCursor\w*|LoadIcon\w*|"
    r"CoInitialize\w*|CoCreateInstance|CoUninitialize|SAFE_RELEASE|_beginthread\w*|_endthread\w*|__declspec|DECLSPEC\w*|STDMETHOD\w*|THIS_?|PURE|REFIID|REFCLSID|GUID|IID_\w+|CLSID_\w+|Mf\w+|MF\w+|IMF\w+|IMedia\w+|IWMS\w+|IWM\w+|"
    r"ImGui_ImplWin32\w*|ImGui_ImplDX9\w*|IDXGI\w*|DXGI\w*|D3DX\w*|XINPUT\w*|XAudio\w*|EAX\w*|_eax\w*|IA3D\w*|CreateWindow\w*|ShowWindow|UpdateWindow|AdjustWindowRect\w*|PeekMessage\w*|GetMessage\w*|TranslateMessage|DispatchMessage\w*|PostQuitMessage|"
    r"LPSTR|LPCSTR|LPCWSTR|LPWSTR|LPVOID|LPCVOID|DWORD_PTR|ULONG_PTR|PVOID|HANDLE|HMODULE|BOOLEAN|PBYTE|LPBYTE|DWORD|LONG|ULONG|WORD|BYTE|BOOL|UINT|INT|MAX_PATH|INFINITE|TRUE|FALSE|WINAPI|CALLBACK|APIENTRY|INVALID_HANDLE_VALUE|STILL_ACTIVE|WAIT_\w+|FILE_\w+|GENERIC_\w+|OPEN_\w+|CREATE_\w+|PAGE_\w+|MEM_\w+|THREAD_\w+|PROCESS_\w+|ERROR_\w+|E_\w+|S_\w+|HRESULT_\w+|FACILITY_\w+|SEVERITY_\w+|FAILED|SUCCEEDED|MAKE_HRESULT|LOWORD|HIWORD|MAKELONG|MAKEWORD|MAKELPARAM|RGB)\b")

CRT_NAMES = re.compile(r"\b(_?stricmp|_?strnicmp|_?strlwr|_?strupr|_?strdup|_?strrev|strcpy_s|strcat_s|sprintf_s|vsprintf_s|_snprintf\w*|_vsnprintf\w*|_?itoa|_?ltoa|_?ultoa|_?gcvt|_?fcvt|_?ecvt|_?access|_?mkdir|_?rmdir|_?chdir|_?getcwd|_?unlink|_?fileno|_?filelength|_?fullpath|_?splitpath\w*|_?makepath\w*|_?finite|_?isnan|_?chgsign|_?copysign|_?hypot|_?clearfp|_?controlfp\w*|_?control87|_?statusfp|_?fpreset|_?set_SSE2_enable|_?aligned_malloc|_?aligned_free|_?aligned_realloc|_?msize|_?expand|_?heapchk|_?CrtDbg\w*|_?CrtSet\w*|_?ASSERT\w*|_?fseeki64|_?ftelli64|_?stat\w*|_?findfirst\w*|_?findnext\w*|_?findclose|_finddata\w*|_?wcsicmp|_?wcsdup|_?mbs\w+|_?tcs\w+|_?tfopen|_?wfopen|fopen_s|fscanf_s|sscanf_s|scanf_s|_?open|_?close|_?read|_?write|_?lseek|_?tell|_?setmode|_?O_\w+|_?S_I\w+|_?rotl\w*|_?rotr\w*|_?byteswap_\w+|_?BitScan\w+|_?InterlockedExchange\w*|_?ReturnAddress|_?AddressOfReturnAddress|__debugbreak|__nop|__cpuid\w*|__rdtsc|__readeflags|__int8|__int16|__int32|__int64|__w64|__pragma|_?alloca|_?malloca|_?freea|_countof|_ARRAYSIZE|_?abs64|_?atoi64|_?strtoi64|_?strtoui64|_?time64|_?localtime64|_?gmtime64|_?mktime64|_?ftime\w*|_?timeb|_?tzset|_?environ|_?putenv|_?getenv_s|_?dupenv_s|_?exit|_?onexit|_?atexit|_?set_new_handler|_?set_purecall_handler|_?set_invalid_parameter_handler|_?set_abort_behavior|_?set_error_mode|_?splitpath_s|_?makepath_s|_?wsplitpath|_?wmakepath|_?wfullpath|_?strtime|_?strdate|_?ctime64|_?isatty|_?dup2?|_?pipe|_?spawn\w*|_?exec\w*|_?cwait|_?getpid|_?sleep|_?cexit|_?c_exit|_?Exit|_?CRT_\w+|_?MAX_\w+|_?MSC_\w+|_?M_\w+)\b")

SIMD = re.compile(r"\b(_mm_\w+|__m128\w*|__m256\w*|__m64|_MM_\w+|_mm\w+|xmm\w*|__builtin_ia32\w*|__asm\w*|__fld\w*|__fst\w*|__emit|__naked|naked|_asm|__readfsdword|__readgsqword|__writefsdword|__outbyte|__inbyte)\b")


def classify(mode, file, msg):
    m = msg
    if 'static assertion failed' in m or 'static_assert' in m:
        if re.search(r'sizeof|offsetof|VALIDATE|Invalid structure size|Invalid offset|size', m, re.I):
            return 'P: 64-bit layout (sizeof/offsetof static_assert)'
        return 'other static_assert'
    if re.search(r"unsupported architecture '[^']*' for MS-style inline assembly", m):
        return 'F/C: inline asm / naked'
    if re.search(r"no member named '(enumerate|shift_right|shift_left|zip|stride|chunk|slide|join_with|to|as_const|adjacent|cartesian_product)' in namespace 'std|no type named 'stacktrace' in namespace 'std|<stacktrace>|std::expected|is unavailable: introduced in macOS", m):
        return 'C: libc++ / standard-library gap'
    if re.search(r"'operator new' takes type size_t|'operator delete' takes type", m):
        return 'P: 64-bit (size_t in operator new/delete)'
    if re.search(r"no matching function for call to '(min|max|clamp|GetRandomNumberInRange|lerp|abs|exchange)'", m):
        return 'P: integer width / mixed-type overload'
    if re.search(r'asm|naked|x87|__declspec\(naked\)|inline assembler|unknown directive|invalid instruction|unexpected token in argument list|invalid operand|unknown token in expression', m):
        return 'F/C: inline asm / naked'
    if re.search(r'__thiscall|__fastcall|__stdcall|__cdecl|calling convention|callconv', m):
        return 'C: calling convention'
    mm = re.search(r"(?:use of undeclared identifier|unknown type name|no member named|no type named|undeclared|no template named|use of undeclared)\s+'([^']+)'", m)
    name = mm.group(1) if mm else None
    if "file not found" in m:
        h = re.search(r"'([^']+)' file not found", m)
        hn = h.group(1) if h else ''
        if re.search(r'd3d|dsound|dinput|windows|winsock|mmsystem|mf\w*\.h|xinput|dwmapi|comdef|shlobj|objbase|ole2|crtdbg|intrin|d3dx|xaudio|xtl|dshow|strmif|evr|vmr|uuids|eax|process\.h|io\.h|direct\.h|conio|malloc\.h|unknwn|mferror|mfidl|mfapi|mfreadwrite|wmsdk|mmreg|ks\w*\.h|msacm', hn, re.I):
            return 'W: Windows/DirectX header'
        return 'other: header not found (%s)' % hn
    if name:
        if SIMD.search(name):
            return 'F/C: SIMD / intrinsic / asm'
        if CRT_NAMES.search(name):
            return 'W/C: MSVC CRT name'
        if WIN_NAMES.fullmatch(name) or WIN_NAMES.search(name):
            if re.match(r'(D3D|IDirect|LPDIRECT|DS|DI|WAVE|EAX|IDXGI|XINPUT|Rw?D3D|_rwD3D|rwD3D)', name) or 'D3D' in name or 'Direct' in name:
                return 'W/R: DirectX'
            return 'W: Win32 API / type'
        if re.match(r'^(_?rw|Rw|Rp|Rt|Rx|RW)', name) or 'rw::' in name:
            return 'R: RenderWare API'
        return 'other: undeclared/unknown %s' % ('type' if 'type name' in m else 'identifier')
    if re.search(r'cast to smaller integer type|cast to .* from smaller integer type|cast from pointer to smaller type|cast to pointer from integer of different size|pointer to int of different size|truncat|non-constant-expression cannot be narrowed|constant expression evaluates to .* which cannot be narrowed|type .* cannot be narrowed|narrowing', m):
        return 'P: pointer/int width (cast/narrowing)'
    if re.search(r'incompatible pointer|cannot initialize .* with an (lvalue|rvalue)|cannot convert|no viable (conversion|constructor|function)|no matching', m):
        return 'other: conversion/overload'
    if re.search(r'default member initializer .* needed within definition', m):
        return 'C: nested DMI inside enclosing class'
    if re.search(r'typename|template keyword|dependent|two-phase|before its definition|incomplete type|deduced return type|no member named .* in .*<', m):
        return 'C: two-phase lookup / template / incomplete type'
    if re.search(r"expected|unexpected|extraneous|unknown (attribute|argument)|invalid|use of .* is a Microsoft extension|ISO C\+\+", m):
        return 'C: syntax / extension'
    return 'other'


def stream_of(cat, f):
    """owner stream of a unique diagnostic: by file first (the code area belongs to a stream), else by category"""
    if re.search(r'^source/(standalone/rw/|fakerw/|game_sa/Pipelines/|game_sa/RenderWare/|vendor/librw|game_sa/Gamma|game_sa/RealTimeShadow)', f) or 'librw' in f or 'rwd3d' in f.lower():
        return 'R render'
    if re.search(r'^source/(app/platform/|app/|oswrapper/|game_sa/Audio/Hardware/|game_sa/Audio/Loaders/|game_sa/CdStream|game_sa/VideoPlayer|game_sa/FileMgr|game_sa/WinInput|game_sa/Pad|extensions/File)', f):
        return 'W platform'
    if re.search(r'^source/(reversiblehooks/|PluginBase|standalone/(Fixups|DataImage|GlobalsVerify)|toolsmenu/DebugModules/HooksDebugModule|InjectHooksMain)', f):
        return 'A fixed addresses / hooks'
    if re.search(r'X87|FtolExe|xmmintrin|emmintrin|mmintrin|arm_neon|pipeline_skin_(cpu_)?core', f) or cat.startswith('F/C'):
        return 'F float / x87 / SIMD'
    if cat.startswith('P:'):
        return 'P 64-bit'
    if cat.startswith('C: libc++'):
        return 'C compiler / standard library'
    if cat.startswith('W'):
        return 'W platform' if not cat.startswith('W/R') else 'R render'
    if cat.startswith('C:'):
        return 'C compiler'
    return 'unclassified (cascade / other)'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--mode', choices=['msvc', 'native'], required=True)
    ap.add_argument('--top', type=int, default=20)
    ap.add_argument('--tsv')
    a = ap.parse_args()
    logd = os.path.join(a.out, 'logs')
    tus = {}
    raw = collections.Counter()
    uniq = {}          # (file,line,msg) -> category
    per_tu = collections.Counter()
    per_tu_cat = collections.defaultdict(collections.Counter)
    per_file = collections.Counter()      # unique diagnostics by the file they point to
    per_file_cat = collections.defaultdict(collections.Counter)
    raw_cat = collections.Counter()
    cat_tus = collections.defaultdict(set)
    cat_files = collections.defaultdict(set)
    stream_tus = collections.defaultdict(set)
    stream_files = collections.defaultdict(set)
    logs = [(n, os.path.join(logd, n)) for n in sorted(os.listdir(logd))]
    pchlog = os.path.join(a.out, 'pch.log')
    if os.path.exists(pchlog):   # errors inside the shared PCH (StdInc.h closure) are counted once, as the pseudo TU '<PCH>'
        logs.append(('<PCH>', pchlog))
    for n, path in logs:
        txt = open(path, errors='replace').read()
        m = re.match(r'# (.*?) rc=(-?\d+)', txt)
        if m:
            tu, rc = m.group(1), int(m.group(2))
        else:
            tu, rc = '<PCH StdInc.h closure>', (1 if re.search(r': (fatal )?error: ', txt) else 0)
        seen = set()
        for line in txt.split('\n'):
            d = DIAG.match(line)
            if not d:
                continue
            f = d['file'].replace(REPO, '')
            msg = re.sub(r'\s*\[-W[\w-]+\]$', '', d['msg'])
            cat = classify(a.mode, f, msg)
            key = (f, int(d['line']), msg)
            raw[cat] += 1
            if (f, int(d['line']), msg, ) in seen:
                continue
            seen.add(key)
            per_tu[tu] += 1
            per_tu_cat[tu][cat] += 1
            cat_tus[cat].add(tu); cat_files[cat].add(f)
            st = stream_of(cat, f); stream_tus[st].add(tu); stream_files[st].add(f)
            if key not in uniq:
                uniq[key] = cat
                per_file[f] += 1
                per_file_cat[f][cat] += 1
        tus[tu] = rc
    real = [t for t in tus if not t.startswith('<PCH')]     # the PCH closure is reported as a pseudo TU of its own
    nclean = sum(1 for t in real if per_tu[t] == 0 and tus[t] == 0)
    fatal = sum(1 for t in real if per_tu[t] == 0 and tus[t] != 0)
    print('## Totals (%s)\n' % a.mode)
    print('| item | count |\n|---|---|')
    print('| TUs | %d |' % len(real))
    print('| TUs clean | %d |' % nclean)
    print('| TUs with errors | %d |' % sum(1 for t in real if per_tu[t] > 0))
    print('| errors inside the shared PCH closure (StdInc.h), counted once | %d |' % per_tu.get('<PCH StdInc.h closure>', 0))
    print('| TUs failed without parsed diagnostics (crash/timeout) | %d |' % fatal)
    print('| raw diagnostics (every re-parse counted) | %d |' % sum(raw.values()))
    print('| unique diagnostics (file,line,message) | %d |' % len(uniq))
    print('\n## By category (unique | raw)\n')
    uc = collections.Counter(uniq.values())
    print('| category | unique | raw | files | TUs reaching it |\n|---|---|---|---|---|')
    for c, n in uc.most_common():
        print('| %s | %d | %d | %d | %d |' % (c, n, raw[c], len(cat_files[c]), len(cat_tus[c])))
    print('\n## By owner stream (unique diagnostics; stream chosen by file area first, then category)\n')
    sc = collections.Counter(stream_of(c, f) for (f, l, m), c in uniq.items())
    print('| stream | unique | files | TUs reaching it |\n|---|---|---|---|')
    for c, n in sc.most_common():
        print('| %s | %d | %d | %d |' % (c, n, len(stream_files[c]), len(stream_tus[c])))
    print('\n## Top %d files by unique diagnostics (file the diagnostic points to)\n' % a.top)
    print('| file | unique | main categories |\n|---|---|---|')
    for f, n in per_file.most_common(a.top):
        cats = ', '.join('%s %d' % (c[:34], k) for c, k in per_file_cat[f].most_common(3))
        print('| %s | %d | %s |' % (f, n, cats))
    print('\n## Top %d TUs by diagnostics (headers re-counted per TU)\n' % a.top)
    print('| TU | diagnostics |\n|---|---|')
    for t, n in per_tu.most_common(a.top):
        print('| %s | %d |' % (t.replace(REPO, ''), n))
    if a.tsv:
        with open(a.tsv, 'w') as o:
            o.write('category\tfile\tline\tmsg\n')
            for (f, l, m), c in sorted(uniq.items()):
                o.write('%s\t%s\t%d\t%s\n' % (c, f, l, m.replace('\t', ' ')))
    # most frequent messages by category "other"
    print('\n## Most frequent messages (unique, normalised) in the biggest categories\n')
    msgc = collections.defaultdict(collections.Counter)
    for (f, l, m), c in uniq.items():
        nm = re.sub(r"'[^']*'", "'…'", m)
        msgc[c][nm] += 1
    for c, _ in uc.most_common(8):
        print('### %s' % c)
        for nm, k in msgc[c].most_common(6):
            print('- %d x %s' % (k, nm[:160]))
        print()


if __name__ == '__main__':
    main()
