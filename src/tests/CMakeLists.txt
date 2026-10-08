# GTEST
find_package(GTest)

if(${GTEST_FOUND})
  include(GoogleTest)
  enable_testing()

  add_executable(lpc_tests test_lpc.cc test_egc.cc)
  target_link_libraries(lpc_tests PRIVATE ${FLUFFOS_LINK} GTest::GTest  GTest::Main)
  target_compile_definitions(lpc_tests PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")

  add_executable(compile_arena_tests test_compile_arena.cc)
  target_link_libraries(compile_arena_tests PRIVATE ${FLUFFOS_LINK} GTest::GTest GTest::Main)

  add_executable(ofile_tests test_ofile.cc)
  target_link_libraries(ofile_tests PRIVATE ${FLUFFOS_LINK} GTest::GTest  GTest::Main)
  target_compile_definitions(ofile_tests PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")

  gtest_discover_tests(lpc_tests
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "lpc_tests."
  )
  gtest_discover_tests(ofile_tests
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "ofile_tests."
  )
  gtest_discover_tests(compile_arena_tests
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "compile_arena_tests."
  )

  # GNU/LLVM ELF link wrapping injects failures only into this test executable.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(object_store_tests test_object_store_summary.cc)
    target_link_libraries(object_store_tests PRIVATE ${FLUFFOS_LINK} GTest::GTest GTest::Main)
    target_link_options(object_store_tests PRIVATE "-Wl,--wrap=_Znwm")
    gtest_discover_tests(object_store_tests
      DISCOVERY_TIMEOUT 60
      TEST_PREFIX "object_store_tests."
    )
    add_executable(file_io_tests test_file_io.cc)
    target_link_libraries(file_io_tests PRIVATE ${FLUFFOS_LINK} GTest::GTest GTest::Main)
    target_compile_definitions(file_io_tests PRIVATE
      -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
    foreach(symbol open close unlink rename fstat ftruncate fdopen gzdopen fopen fclose fread
                   fwrite fprintf fputs fputc ferror gzopen gzread gzwrite gzprintf gzclose
                   posix_spawn posix_spawn_file_actions_init posix_spawn_file_actions_destroy
                   posix_spawn_file_actions_adddup2)
      target_link_options(file_io_tests PRIVATE "-Wl,--wrap=${symbol}")
    endforeach()
    add_executable(trace_io_probe trace_io_probe.cc)
    target_link_libraries(trace_io_probe PRIVATE ${FLUFFOS_LINK})
    target_compile_definitions(trace_io_probe PRIVATE
      -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
    target_link_options(trace_io_probe PRIVATE
      "-Wl,--wrap=_Z13debug_messagePKcz" "-Wl,--wrap=_Znwm")
    add_executable(compound_free_probe compound_free_probe.cc)
    target_link_libraries(compound_free_probe PRIVATE ${FLUFFOS_LINK})
    target_link_options(compound_free_probe PRIVATE
      "-Wl,--wrap=_Znwm" "-Wl,--wrap=_ZdlPv" "-Wl,--wrap=_ZdlPvm")
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    add_test(NAME trace_io_tests
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tools/testsuite/test-trace-io.py"
        --probe $<TARGET_FILE:trace_io_probe> --driver $<TARGET_FILE:driver>
    )
    add_test(NAME file_io_tests
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tools/testsuite/run-targeted.py"
        --binary $<TARGET_FILE:file_io_tests> --gtest-filter=FileIoTest.* --timeout 90
    )
    add_test(NAME lpcc_io_tests
      COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_SOURCE_DIR}/tools/testsuite/test-lpcc-io.py"
        --lpcc $<TARGET_FILE:lpcc>
    )
  endif()
endif()

add_executable(owner_runtime_bench owner_runtime_bench.cc)
add_executable(bench_compile bench_compile.cc)
add_executable(bench_explode bench_explode.cc)
add_executable(bench_scratchpad bench_scratchpad.cc)
target_link_libraries(owner_runtime_bench PRIVATE ${FLUFFOS_LINK})
target_link_libraries(bench_compile PRIVATE ${FLUFFOS_LINK})
target_link_libraries(bench_explode PRIVATE ${FLUFFOS_LINK})
target_compile_definitions(bench_explode PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
target_link_libraries(bench_scratchpad PRIVATE ${FLUFFOS_LINK})
# T3.3: structured diagnostic rendering baseline.
add_executable(bench_diagnostic_render bench_diagnostic_render.cc)
target_link_libraries(bench_diagnostic_render PRIVATE ${FLUFFOS_LINK})

# Gateway frame parser fuzz targets (T15/F08/R2-F02).
#
# Default build graph: only gateway_fuzz_smoke (real main() from the shim).
# The libFuzzer target gateway_fuzz is created EXCLUSIVELY when
# GATEWAY_FUZZ_LIBFUZZER=ON with clang (the fuzzer runtime provides main),
# so plain `make all/install` and CodeQL builds never link a no-main target.
if(GATEWAY_FUZZ_LIBFUZZER AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "GATEWAY_FUZZ_LIBFUZZER=ON requires a Clang compiler for "
    "-fsanitize=fuzzer (found ${CMAKE_CXX_COMPILER_ID} "
    "${CMAKE_CXX_COMPILER_VERSION}). Configure the default build without "
    "this option, or reconfigure with clang.")
endif()

add_library(gateway_fuzz_core OBJECT gateway_fuzz.cc)
target_compile_definitions(gateway_fuzz_core PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
target_link_libraries(gateway_fuzz_core PRIVATE ${FLUFFOS_LINK})

add_executable(gateway_fuzz_smoke gateway_fuzz_smoke_main.cc)
target_link_libraries(gateway_fuzz_smoke PRIVATE gateway_fuzz_core ${FLUFFOS_LINK})
target_compile_definitions(gateway_fuzz_smoke PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")

if(GATEWAY_FUZZ_LIBFUZZER)
  add_executable(gateway_fuzz gateway_fuzz.cc)
  target_link_libraries(gateway_fuzz PRIVATE ${FLUFFOS_LINK})
  target_compile_definitions(gateway_fuzz PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
  target_compile_options(gateway_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
  target_link_options(gateway_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
target_compile_definitions(owner_runtime_bench PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
target_compile_definitions(bench_compile PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")

add_executable(lpc_vm_bench lpc_vm_bench.cc)
target_link_libraries(lpc_vm_bench PRIVATE ${FLUFFOS_LINK})
target_compile_definitions(lpc_vm_bench PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")

add_executable(object_store_bench object_store_bench.cc)
target_link_libraries(object_store_bench PRIVATE ${FLUFFOS_LINK})
target_compile_definitions(object_store_bench PRIVATE -DTESTSUITE_DIR="${CMAKE_SOURCE_DIR}/testsuite")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(object_store_bench PRIVATE
    "-Wl,--wrap=pthread_rwlock_rdlock" "-Wl,--wrap=pthread_rwlock_unlock" "-Wl,--wrap=_Znwm")
endif()

add_executable(gateway_full_wire_bench gateway_full_wire_bench.cc)
target_link_libraries(gateway_full_wire_bench PRIVATE ${FLUFFOS_LINK})
