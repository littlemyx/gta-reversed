# MAC-FRFIX: portable.file_records green under global NOTSA_FILE_SCALAR_STRICT
- Flag -DNOTSA_FILE_SCALAR_STRICT untouched everywhere (user decision).
- tools/portable/run_file_records_test.sh: old "warn-mode" step (expected 4 warnings) replaced:
  - (1b) positive build with -Wuser-defined-warnings: 0 lines containing the guard text (the "must compile with 0 diagnostics" check).
  - (2) the 4 former warn cases, now tests/portable/file_records_test.cpp -DFR_WARN=1..4 (size_t, ptrdiff_t, void*, unsigned long): each must FAIL to compile,
    with exactly 1 `error: 64-bit-unsafe scalar ...` (the diagnose_if text) and exactly 1 error total; compiles / other errors / count != 1 => fail.
  - (3) FR_NEG=1..6 and the positive run unchanged in meaning (the script now also passes -DNOTSA_FILE_SCALAR_STRICT explicitly, harmless duplicate).
  - FR_WARN_CASES block removed from the test (its 8-byte-records CHECK is moot: it can no longer compile).
- Side fix needed for "-L portable all green": p2c/p2d/p2e/p2f/p2h were red (BUILD FAILED) since F-loop 2 / MAC-C3: the stub tests/portable/p2c_stubs/Base.h lacked
  x87d / X87R / NOTSA_UNREACHABLE_RET used by the real Vector2D.h / Vector.h / Matrix.h. Added them (same defs as source/Base.h).
- Results: `ctest --test-dir build/PortableMac -L portable` -> 100% tests passed, 0 tests failed out of 18; `-L mutation` -> 100% tests passed out of 12.
- Files: tools/portable/run_file_records_test.sh, tests/portable/file_records_test.cpp, tests/portable/p2c_stubs/Base.h, this report.
