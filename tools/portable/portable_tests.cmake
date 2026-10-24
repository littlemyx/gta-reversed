# CTest registration of the portable host tests (P-2k). Included from source/CMakeLists.txt ONLY when GTASA_PORTABLE=ON (the Windows / StandaloneRelease
# configure never sees this file). Run: `ctest --test-dir build/PortableMac -L portable` (fast set, ~30 s) ; `-L mutation` adds the mutant runs (slow, each mutant must make
# its test FAIL). The tests are plain scripts: they build single-TU executables with the flags of this build's compile_commands.json (COMPDB) and do not link the game.
# ASAN_OPTIONS symbolize=0: a crashing/failing mutant must not hang in ASan's `atos` symbolizer when ctest has no tty (seen: p2g/p2i mutants stuck for 30+ min).
# Real-data parts (gta3.img, NODES*.DAT, main.scm of the Steam copy) are skipped with a NOTE when the data is absent (GTA / P2*_ env vars select another copy).
set(PORTABLE_TESTS_DIR "${PROJECT_SOURCE_DIR}/tools/portable")
set(PORTABLE_TESTS_OUT "${CMAKE_BINARY_DIR}/portable_tests")
file(MAKE_DIRECTORY "${PORTABLE_TESTS_OUT}")

set(PORTABLE_HOST_TEST_NAMES   file_records p2b p2c p2d p2e p2f p2g p2h p2i p2j msvc_rand p4_visplugin p4_keyarray p4_restore)
set(PORTABLE_HOST_TEST_SCRIPTS run_file_records_test.sh run_p2b_test.sh run_p2c_test.sh run_p2d_test.sh run_p2e_test.sh run_p2f_test.sh run_p2g_test.sh run_p2h_test.sh run_p2i_test.sh run_p2j_test.sh run_msvc_rand_test.sh run_p4_visplugin_test.sh run_p4_keyarray_test.sh run_p4_restore_test.sh)
# scripts that take a `mutate` argument (source mutants that the test must catch)
set(PORTABLE_MUTATE_TESTS p2f p2g p2h p2i p2j msvc_rand p4_visplugin p4_keyarray p4_restore)

foreach(_name _script IN ZIP_LISTS PORTABLE_HOST_TEST_NAMES PORTABLE_HOST_TEST_SCRIPTS)
    add_test(NAME portable.${_name}
        COMMAND bash "${PORTABLE_TESTS_DIR}/${_script}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(portable.${_name} PROPERTIES
        LABELS "portable"
        TIMEOUT 900
        ENVIRONMENT "COMPDB=${CMAKE_BINARY_DIR}/compile_commands.json;OUT=${PORTABLE_TESTS_OUT}/${_name};ASAN_OPTIONS=detect_leaks=0:symbolize=0"
        FAIL_REGULAR_EXPRESSION "CENSUS MISMATCH;BUILD FAILED")
    if(_name IN_LIST PORTABLE_MUTATE_TESTS)
        add_test(NAME portable.${_name}.mutate
            COMMAND bash "${PORTABLE_TESTS_DIR}/${_script}" mutate
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
        set_tests_properties(portable.${_name}.mutate PROPERTIES
            LABELS "mutation"
            TIMEOUT 3600
            ENVIRONMENT "COMPDB=${CMAKE_BINARY_DIR}/compile_commands.json;OUT=${PORTABLE_TESTS_OUT}/${_name}_mut;ASAN_OPTIONS=detect_leaks=0:symbolize=0")
    endif()
endforeach()

# R-5: every TXD of the Steam copy (or $GTA) -> GL3 rasters. Needs a display session (SDL window + GL context) and the game data; both absent = "TXD RESULT SKIP"
# (exit 0). Own librw build in build/gl_smoke (shared with run_gl_smoke.sh); ~2 s once built.
add_test(NAME portable.txd_gl
    COMMAND bash "${PORTABLE_TESTS_DIR}/run_txd_test.sh"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
set_tests_properties(portable.txd_gl PROPERTIES
    LABELS "portable"
    TIMEOUT 900
    RUN_SERIAL TRUE
    ENVIRONMENT "OUT=${PORTABLE_TESTS_OUT}/txd_gl;COMPDB=${CMAKE_BINARY_DIR}/compile_commands.json"
    FAIL_REGULAR_EXPRESSION "TXD RESULT FAIL;COMPILE FAILED;LINK FAILED")
