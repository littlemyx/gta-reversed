// MAC-W0a: process entry point of the portable build. The game's own entry (app/platform/win/WinMain.cpp `WinMain`, NOTSA_STANDALONE) is called the way the
// Windows CRT does it: with the command line already split into __argc / __argv. (Portable-only file; not part of the Windows targets.)
#include "StdInc.h"

#include "app_win32_compat.h"

int       __argc  = 0;
char**    __argv  = nullptr;
wchar_t** __wargv = nullptr; // W-A7: wide argv is never read on this path (WinMain clears it only to dodge a Windows CRT teardown crash)

INT WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR cmdLine, INT nCmdShow);

int main(int argc, char** argv) {
    __argc = argc;
    __argv = argv;
    return WinMain(nullptr, nullptr, const_cast<LPSTR>(""), SW_SHOWNORMAL);
}
