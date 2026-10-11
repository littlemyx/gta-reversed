Census-only stubs for `clang_census.py --mode native` (arm64 macOS, `-fsyntax-only`). NOT a Windows porting layer:
`Windows.h` declares only the scalar types / calling-convention keywords that make the headers parse; every Win32/DirectX
function, struct and constant stays undeclared on purpose, so each use is counted (and classified) as a stream W/R error.
The other headers are empty files so that `#include` does not abort the translation unit.
