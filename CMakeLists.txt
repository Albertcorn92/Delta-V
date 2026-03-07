cmake_minimum_required(VERSION 3.15)
project(DELTA_V VERSION 4.0.0 LANGUAGES CXX)

# =============================================================================
# Global Settings  (DO-178C: strict ISO C++20, no extensions)
# =============================================================================
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Required by clang-tidy and IDEs

find_package(Threads REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Build type: Debug | Release | RelWithDebInfo" FORCE)
endif()

# =============================================================================
# Compiler Safety Flags  (DO-178C / MISRA C++ influenced)
# =============================================================================
set(DELTAV_WARN_FLAGS
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wconversion          # Warn on implicit narrowing conversions
    -Wshadow              # Local variable shadows outer scope
    -Wnull-dereference    # Potential null dereference
    -Wformat=2            # Strict printf/scanf format checking
)

set(DELTAV_RELEASE_FLAGS
    -O2 -DNDEBUG
)

set(DELTAV_SANITIZER_FLAGS
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -g
)

set(DELTAV_COVERAGE_FLAGS
    --coverage
    -fprofile-arcs
    -ftest-coverage
    -O0 -g
)

# =============================================================================
# Source Discovery
# CONFIGURE_DEPENDS: CMake re-globs if any src/*.cpp appears/disappears
# (dv-util add-component awareness — no manual CMake edits needed)
# =============================================================================
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# =============================================================================
# Target: flight_software
# The SITL binary. On ESP32/FreeRTOS this is replaced by the platform SDK.
# =============================================================================
add_executable(flight_software src/main.cpp ${ALL_SOURCES})
target_include_directories(flight_software PRIVATE src)
target_link_libraries(flight_software PRIVATE Threads::Threads)
target_compile_options(flight_software PRIVATE ${DELTAV_WARN_FLAGS})

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(flight_software PRIVATE ${DELTAV_RELEASE_FLAGS})
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT APPLE AND NOT MSVC)
        target_compile_options(flight_software PRIVATE ${DELTAV_SANITIZER_FLAGS})
        target_link_options(flight_software   PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# =============================================================================
# Target: run_benchmarks  (software performance baseline)
# =============================================================================
add_executable(run_benchmarks benchmarks/benchmark_main.cpp ${ALL_SOURCES})
target_include_directories(run_benchmarks PRIVATE src)
target_link_libraries(run_benchmarks PRIVATE Threads::Threads)
target_compile_options(run_benchmarks PRIVATE ${DELTAV_WARN_FLAGS})
target_compile_definitions(run_benchmarks PRIVATE DELTAV_FAULT_INJECT)
# Benchmarks are intended for regression/performance gates; keep them on a
# release-like profile independent of the parent build type to avoid sanitizer
# overhead distorting throughput/latency thresholds in CI.
target_compile_options(run_benchmarks PRIVATE ${DELTAV_RELEASE_FLAGS})

# =============================================================================
# Target: run_tests  (Google Test suite)
# Enabled by: cmake --build build --target run_tests
# =============================================================================
enable_testing()

# Homebrew gtest binaries on macOS are commonly built with clang/libc++, which
# conflicts with GNU/libstdc++ coverage builds. In that case, build gtest from
# Homebrew-provided sources with the active compiler instead.
set(DELTAV_BUILD_GTEST_FROM_SOURCE OFF)
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(DELTAV_BUILD_GTEST_FROM_SOURCE ON)
endif()

if(DELTAV_BUILD_GTEST_FROM_SOURCE)
    set(DELTAV_GTEST_PREFIX_CANDIDATES
        "$ENV{HOMEBREW_PREFIX}/opt/googletest"
        "/opt/homebrew/opt/googletest"
        "/usr/local/opt/googletest"
    )
    set(DELTAV_GTEST_PREFIX "")
    foreach(_prefix IN LISTS DELTAV_GTEST_PREFIX_CANDIDATES)
        if(_prefix AND EXISTS "${_prefix}/include/googletest/googletest/src/gtest-all.cc")
            set(DELTAV_GTEST_PREFIX "${_prefix}")
            break()
        endif()
    endforeach()

    if(NOT DELTAV_GTEST_PREFIX)
        message(FATAL_ERROR
            "GNU-on-macOS test build requires Homebrew googletest sources.\n"
            "Install with: brew install googletest"
        )
    endif()

    add_library(deltav_gtest STATIC
        ${DELTAV_GTEST_PREFIX}/include/googletest/googletest/src/gtest-all.cc
    )
    target_include_directories(deltav_gtest
        PUBLIC
            ${DELTAV_GTEST_PREFIX}/include
            ${DELTAV_GTEST_PREFIX}/include/googletest/googletest
    )
    target_link_libraries(deltav_gtest PUBLIC Threads::Threads)

    add_library(deltav_gtest_main STATIC
        ${DELTAV_GTEST_PREFIX}/include/googletest/googletest/src/gtest_main.cc
    )
    target_link_libraries(deltav_gtest_main PUBLIC deltav_gtest)

    add_library(GTest::GTest ALIAS deltav_gtest)
    add_library(GTest::Main ALIAS deltav_gtest_main)
    set(GTest_FOUND TRUE)
    message(STATUS "GTest   : building from source (${DELTAV_GTEST_PREFIX})")
else()
    find_package(GTest QUIET)
endif()

if(GTest_FOUND OR GTEST_FOUND)

    # ── Main test binary ──────────────────────────────────────────────────────
    add_executable(run_tests tests/unit_tests.cpp ${ALL_SOURCES})
    target_include_directories(run_tests PRIVATE src)
    target_link_libraries(run_tests PRIVATE Threads::Threads GTest::GTest GTest::Main)
    target_compile_options(run_tests PRIVATE ${DELTAV_WARN_FLAGS})
    # Compile-time flag that unlocks FaultInjector.hpp in tests
    target_compile_definitions(run_tests PRIVATE DELTAV_FAULT_INJECT)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT APPLE AND NOT MSVC)
        target_compile_options(run_tests PRIVATE ${DELTAV_SANITIZER_FLAGS})
        target_link_options(run_tests   PRIVATE -fsanitize=address,undefined)
    endif()

    add_test(NAME DeltaV_Unit_Tests COMMAND run_tests --gtest_color=yes)
    set_tests_properties(DeltaV_Unit_Tests PROPERTIES TIMEOUT 60)

    add_executable(run_system_tests tests/system_tests.cpp ${ALL_SOURCES})
    target_include_directories(run_system_tests PRIVATE src)
    target_link_libraries(run_system_tests PRIVATE Threads::Threads GTest::GTest GTest::Main)
    target_compile_options(run_system_tests PRIVATE ${DELTAV_WARN_FLAGS})
    target_compile_definitions(run_system_tests PRIVATE DELTAV_FAULT_INJECT)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT APPLE AND NOT MSVC)
        target_compile_options(run_system_tests PRIVATE ${DELTAV_SANITIZER_FLAGS})
        target_link_options(run_system_tests   PRIVATE -fsanitize=address,undefined)
    endif()

    add_test(NAME DeltaV_System_Tests COMMAND run_system_tests --gtest_color=yes)
    set_tests_properties(DeltaV_System_Tests PROPERTIES TIMEOUT 90)

    set(DELTAV_VNV_STRESS_REPEATS "25" CACHE STRING
        "Number of shuffled run_tests repetitions for the V&V stress gate")
    add_custom_target(vnv_stress
        COMMAND run_tests --gtest_color=yes --gtest_shuffle
                --gtest_repeat=${DELTAV_VNV_STRESS_REPEATS}
                --gtest_brief=1
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running V&V stress regression gate (${DELTAV_VNV_STRESS_REPEATS} shuffled repetitions)"
        DEPENDS run_tests
        USES_TERMINAL
    )

    # ── Coverage binary ───────────────────────────────────────────────────────
    add_executable(run_tests_coverage tests/unit_tests.cpp ${ALL_SOURCES})
    target_include_directories(run_tests_coverage PRIVATE src)
    target_link_libraries(run_tests_coverage PRIVATE Threads::Threads GTest::GTest GTest::Main)
    target_compile_options(run_tests_coverage PRIVATE ${DELTAV_COVERAGE_FLAGS})
    target_compile_definitions(run_tests_coverage PRIVATE DELTAV_FAULT_INJECT)
    target_link_options(run_tests_coverage PRIVATE --coverage)

    find_program(LCOV    lcov)
    find_program(GENHTML genhtml)
    unset(DELTAV_GCOV_TOOL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Ask the active compiler for its matching gcov tool to avoid
        # version mismatches (for example, prefixed triplet gcov binaries).
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -print-prog-name=gcov
            OUTPUT_VARIABLE DELTAV_GCOV_FROM_COMPILER
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(DELTAV_GCOV_FROM_COMPILER AND
           NOT DELTAV_GCOV_FROM_COMPILER STREQUAL "gcov")
            if(EXISTS "${DELTAV_GCOV_FROM_COMPILER}")
                set(DELTAV_GCOV_TOOL "${DELTAV_GCOV_FROM_COMPILER}")
            else()
                find_program(DELTAV_GCOV_TOOL
                    NAMES ${DELTAV_GCOV_FROM_COMPILER})
            endif()
        endif()
    endif()
    if(NOT DELTAV_GCOV_TOOL)
        set(DELTAV_GCOV_CANDIDATES gcov)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            string(REGEX MATCH "^[0-9]+" DELTAV_GCC_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
            if(DELTAV_GCC_MAJOR)
                list(PREPEND DELTAV_GCOV_CANDIDATES
                    gcov-${DELTAV_GCC_MAJOR}
                    x86_64-linux-gnu-gcov-${DELTAV_GCC_MAJOR}
                    aarch64-linux-gnu-gcov-${DELTAV_GCC_MAJOR})
            endif()
        endif()
        find_program(DELTAV_GCOV_TOOL NAMES ${DELTAV_GCOV_CANDIDATES})
    endif()
    set(DELTAV_LCOV_GCOV_ARGS)
    if(DELTAV_GCOV_TOOL)
        list(APPEND DELTAV_LCOV_GCOV_ARGS --gcov-tool ${DELTAV_GCOV_TOOL})
    endif()
    if(LCOV AND GENHTML AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(DELTAV_LCOV_REMOVE_PATTERNS
            '/usr/*'
            '*/tests/*'
            '*/googletest/*')
        set(DELTAV_LCOV_CAPTURE_IGNORE_ARGS)
        set(DELTAV_LCOV_REMOVE_IGNORE_ARGS)
        if(APPLE)
            if(EXISTS "/opt/homebrew")
                list(APPEND DELTAV_LCOV_REMOVE_PATTERNS '/opt/homebrew/*')
            endif()
            if(EXISTS "/Library/Developer")
                list(APPEND DELTAV_LCOV_REMOVE_PATTERNS '/Library/Developer/*')
            endif()
            # Homebrew lcov/gcov on macOS can emit toolchain metadata mismatches
            # and unused-exclude warnings; tolerate those only on Apple hosts.
            set(DELTAV_LCOV_CAPTURE_IGNORE_ARGS
                --ignore-errors inconsistent,mismatch,deprecated)
            set(DELTAV_LCOV_REMOVE_IGNORE_ARGS
                --ignore-errors inconsistent,mismatch,deprecated,unused)
        endif()
        add_custom_target(coverage
            COMMAND ${LCOV}    ${DELTAV_LCOV_GCOV_ARGS} --zerocounters
                               --directory . --quiet
            COMMAND run_tests_coverage --gtest_color=yes
            COMMAND ${LCOV}    ${DELTAV_LCOV_GCOV_ARGS} --capture --directory .
                               --rc branch_coverage=1
                               ${DELTAV_LCOV_CAPTURE_IGNORE_ARGS}
                               --output-file cov_raw.info --quiet
            COMMAND ${LCOV}    ${DELTAV_LCOV_GCOV_ARGS} --remove  cov_raw.info
                               ${DELTAV_LCOV_REMOVE_PATTERNS}
                               --rc branch_coverage=1
                               ${DELTAV_LCOV_REMOVE_IGNORE_ARGS}
                               --output-file cov.info --quiet
            COMMAND ${GENHTML} cov.info --branch-coverage
                               --output-directory coverage_html --quiet
            COMMAND ${CMAKE_COMMAND} -E echo
                "Coverage report → ${CMAKE_BINARY_DIR}/coverage_html/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating lcov HTML coverage report"
            DEPENDS run_tests_coverage
        )
        if(DELTAV_GCOV_TOOL)
            message(STATUS "Coverage gcov tool: ${DELTAV_GCOV_TOOL}")
        else()
            message(WARNING "Coverage gcov tool not found explicitly; lcov default gcov will be used.")
        endif()
        message(STATUS "Coverage : ENABLED  (cmake --build build --target coverage)")
    elseif(LCOV AND GENHTML)
        message(STATUS "Coverage : DISABLED (configure with GCC to enable lcov coverage)")
    endif()

    message(STATUS "Tests    : ENABLED  (cmake --build build --target run_tests)")
    message(STATUS "SysTests : ENABLED  (cmake --build build --target run_system_tests)")
    message(STATUS "V&VStress: ENABLED  (cmake --build build --target vnv_stress)")

else()
    message(WARNING
        " GTest not found — unit tests disabled.\n"
        "   Ubuntu/Debian : sudo apt install libgtest-dev cmake\n"
        "   macOS         : brew install googletest")
endif()

# =============================================================================
# Target: tidy  (clang-tidy static analysis)
# =============================================================================
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    set(TIDY_EXTRA_ARGS)
    if(APPLE)
        execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE MACOSX_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(MACOSX_SDK_PATH)
            list(APPEND TIDY_EXTRA_ARGS
                --extra-arg=-isysroot
                --extra-arg=${MACOSX_SDK_PATH}
            )
        endif()
    endif()

    file(GLOB_RECURSE TIDY_FILES CONFIGURE_DEPENDS "src/*.cpp")

    set(DELTAV_TIDY_CHECKS
        cppcoreguidelines-*,cert-*,performance-*,modernize-*,bugprone-*)
    set(DELTAV_TIDY_SAFETY_CHECKS
        -*,clang-analyzer-*,bugprone-infinite-loop,bugprone-use-after-move,bugprone-sizeof-expression,cert-err33-c)

    add_custom_target(tidy
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            --checks=${DELTAV_TIDY_CHECKS}
            --header-filter=^${CMAKE_SOURCE_DIR}/src/.*
            ${TIDY_EXTRA_ARGS}
            ${TIDY_FILES}
        COMMENT "Running clang-tidy static analysis"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_custom_target(tidy_safety
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            --checks=${DELTAV_TIDY_SAFETY_CHECKS}
            --warnings-as-errors=*
            --header-filter=^${CMAKE_SOURCE_DIR}/src/.*
            ${TIDY_EXTRA_ARGS}
            ${TIDY_FILES}
        COMMENT "Running blocking clang-tidy safety gate"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    message(STATUS "Tidy     : ENABLED  (cmake --build build --target tidy)")
    message(STATUS "TidySafe : ENABLED  (cmake --build build --target tidy_safety)")
endif()

# =============================================================================
# Target: gen  (run the DELTA-V autocoder)
# Regenerates src/Types.hpp, src/TopologyManager.hpp, dictionary.json
# from topology.yaml whenever the topology changes.
# =============================================================================
find_program(PYTHON3 python3)
if(PYTHON3)
    add_custom_target(gen
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/autocoder.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "DELTA-V autocoder: topology.yaml → src/TopologyManager.hpp + dictionary.json"
    )
    add_custom_target(legal_compliance
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/legal_compliance_check.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running civilian-use legal compliance checks"
    )
    add_custom_target(traceability
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/requirements_trace.py
            --requirements ${CMAKE_SOURCE_DIR}/src/Requirements.hpp
            --tests ${CMAKE_SOURCE_DIR}/tests/unit_tests.cpp
            --mapping ${CMAKE_SOURCE_DIR}/docs/REQUIREMENTS_TRACE.yaml
            --output-md ${CMAKE_BINARY_DIR}/requirements_trace_matrix.md
            --output-json ${CMAKE_BINARY_DIR}/requirements_trace_matrix.json
            --strict
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating and validating requirements traceability matrix"
    )

    message(STATUS "Autocoder: ENABLED  (cmake --build build --target gen)")
    message(STATUS "Legal    : ENABLED  (cmake --build build --target legal_compliance)")
    message(STATUS "Trace    : ENABLED  (cmake --build build --target traceability)")
endif()

if(TARGET run_tests AND TARGET run_system_tests AND TARGET vnv_stress AND TARGET tidy_safety AND TARGET traceability AND TARGET legal_compliance)
    add_custom_target(flight_readiness
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target legal_compliance
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_tests
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_system_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target vnv_stress
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target tidy_safety
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target traceability
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target flight_software
        COMMENT "Running DELTA-V flight readiness gates"
        USES_TERMINAL
    )
    message(STATUS "Readiness: ENABLED  (cmake --build build --target flight_readiness)")
endif()

if(PYTHON3 AND TARGET flight_readiness)
    add_custom_target(qualification_bundle
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/qualification_report.py
            --workspace ${CMAKE_SOURCE_DIR}
            --trace-json ${CMAKE_BINARY_DIR}/requirements_trace_matrix.json
            --trace-md ${CMAKE_BINARY_DIR}/requirements_trace_matrix.md
            --tests-file ${CMAKE_SOURCE_DIR}/tests/unit_tests.cpp
            --flight-bin ${CMAKE_BINARY_DIR}/flight_software
            --tests-bin ${CMAKE_BINARY_DIR}/run_tests
            --output-dir ${CMAKE_BINARY_DIR}/qualification
        DEPENDS flight_readiness
        COMMENT "Generating qualification evidence bundle"
    )
    message(STATUS "Qualify  : ENABLED  (cmake --build build --target qualification_bundle)")
endif()

if(PYTHON3 AND TARGET qualification_bundle)
    add_custom_target(software_final
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/software_final_check.py
            --workspace ${CMAKE_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
            --python-exe ${PYTHON3}
        DEPENDS qualification_bundle
        COMMENT "Running software-final release gate and syncing docs artifacts"
    )
    message(STATUS "SoftFinal: ENABLED  (cmake --build build --target software_final)")
endif()

if(PYTHON3 AND TARGET coverage)
    add_custom_target(coverage_guard
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/coverage_guard.py
            --workspace ${CMAKE_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
        DEPENDS coverage
        COMMENT "Validating coverage thresholds"
    )
    message(STATUS "CovGuard : ENABLED  (cmake --build build --target coverage_guard)")

    add_custom_target(coverage_trend
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/coverage_trend.py
            --workspace ${CMAKE_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
        DEPENDS coverage
        COMMENT "Generating coverage trend snapshot"
    )
    message(STATUS "CovTrend : ENABLED  (cmake --build build --target coverage_trend)")
endif()

if(PYTHON3 AND TARGET run_benchmarks)
    add_custom_target(benchmark_baseline
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/benchmark_report.py
            --workspace ${CMAKE_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
        DEPENDS run_benchmarks
        COMMENT "Generating benchmark baseline artifacts"
    )
    message(STATUS "BenchRpt : ENABLED  (cmake --build build --target benchmark_baseline)")
endif()

if(PYTHON3 AND TARGET benchmark_baseline)
    set(DELTAV_BENCHMARK_THRESHOLDS_FILE
        "${CMAKE_SOURCE_DIR}/docs/BENCHMARK_THRESHOLDS.json"
        CACHE FILEPATH "Benchmark threshold JSON file for benchmark_guard")
    add_custom_target(benchmark_guard
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/benchmark_guard.py
            --workspace ${CMAKE_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
            --thresholds ${DELTAV_BENCHMARK_THRESHOLDS_FILE}
        DEPENDS benchmark_baseline
        COMMENT "Validating benchmark regression thresholds"
    )
    message(STATUS "BenchGte : ENABLED  (cmake --build build --target benchmark_guard)")
endif()

if(PYTHON3 AND TARGET flight_software)
    add_custom_target(sitl_smoke
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/sitl_smoke.py
            --build-dir ${CMAKE_BINARY_DIR}
            --duration 8
        DEPENDS flight_software
        COMMENT "Running short SITL smoke test"
    )
    message(STATUS "SITLSmoke: ENABLED  (cmake --build build --target sitl_smoke)")

    set(DELTAV_SITL_SOAK_SECONDS "180" CACHE STRING
        "Duration in seconds for the SITL soak gate")
    add_custom_target(sitl_soak
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/sitl_soak.py
            --build-dir ${CMAKE_BINARY_DIR}
            --duration ${DELTAV_SITL_SOAK_SECONDS}
        DEPENDS flight_software
        COMMENT "Running extended SITL soak (${DELTAV_SITL_SOAK_SECONDS}s)"
    )
    message(STATUS "SITLSoak : ENABLED  (cmake --build build --target sitl_soak)")
endif()

if(PYTHON3)
    set(DELTAV_ESP32_PORT "/dev/cu.usbmodem101" CACHE STRING
        "Serial port for optional ESP32 hardware validation commands")
    set(DELTAV_ESP32_RUNTIME_SECONDS "300" CACHE STRING
        "Duration in seconds for ESP32 runtime WCET/stack guard")
    set(DELTAV_ESP32_REBOOT_CYCLES "10" CACHE STRING
        "Cycle count for ESP32 reboot campaign")
    set(DELTAV_ESP32_REBOOT_SECONDS "12" CACHE STRING
        "Per-cycle monitor duration for ESP32 reboot campaign")

    add_custom_target(esp32_runtime_guard
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/esp32_runtime_guard.py
            --project-dir ${CMAKE_SOURCE_DIR}/ports/esp32
            --build-dir build_esp32
            --port ${DELTAV_ESP32_PORT}
            --duration ${DELTAV_ESP32_RUNTIME_SECONDS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running optional ESP32 runtime WCET/stack-margin guard"
        USES_TERMINAL
    )

    add_custom_target(esp32_reboot_campaign
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/tools/esp32_reboot_campaign.py
            --project-dir ${CMAKE_SOURCE_DIR}/ports/esp32
            --build-dir build_esp32
            --port ${DELTAV_ESP32_PORT}
            --cycles ${DELTAV_ESP32_REBOOT_CYCLES}
            --cycle-seconds ${DELTAV_ESP32_REBOOT_SECONDS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running optional ESP32 reboot stability campaign"
        USES_TERMINAL
    )

    message(STATUS "ESP32Run : ENABLED  (cmake --build build --target esp32_runtime_guard)")
    message(STATUS "ESP32Reb : ENABLED  (cmake --build build --target esp32_reboot_campaign)")
endif()

if(TARGET legal_compliance AND TARGET run_tests AND TARGET run_system_tests AND TARGET benchmark_guard AND TARGET sitl_smoke)
    add_custom_target(quickstart_10min
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target legal_compliance
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_tests
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_system_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure --timeout 90
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target benchmark_guard
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target sitl_smoke
        COMMENT "Running 10-minute local quickstart validation flow"
        USES_TERMINAL
    )
    message(STATUS "Quick10m : ENABLED  (cmake --build build --target quickstart_10min)")
endif()

if(TARGET software_final AND TARGET benchmark_guard)
    add_dependencies(software_final benchmark_guard)
endif()

if(TARGET software_final AND TARGET sitl_soak)
    add_dependencies(software_final sitl_soak)
endif()

if(TARGET software_final AND TARGET coverage_guard)
    add_dependencies(software_final coverage_guard)
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "╔════════════════════════════════════════════╗")
message(STATUS "║   DELTA-V  v${PROJECT_VERSION}  Build Summary         ║")
message(STATUS "╠════════════════════════════════════════════╣")
message(STATUS "║  C++ Standard : C++${CMAKE_CXX_STANDARD}                          ║")
message(STATUS "║  Build Type   : ${CMAKE_BUILD_TYPE}                        ║")
message(STATUS "║  Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "╠════════════════════════════════════════════╣")
message(STATUS "║  Targets:                                  ║")
message(STATUS "║    flight_software  — main SITL binary     ║")
message(STATUS "║    run_benchmarks   — software benchmarks  ║")
message(STATUS "║    run_tests        — GTest suite          ║")
message(STATUS "║    run_system_tests — integration tests    ║")
message(STATUS "║    vnv_stress       — stress test gate     ║")
message(STATUS "║    coverage         — lcov HTML report     ║")
message(STATUS "║    coverage_guard   — coverage quality gate║")
message(STATUS "║    coverage_trend   — coverage trend json  ║")
message(STATUS "║    tidy             — clang-tidy analysis  ║")
message(STATUS "║    tidy_safety      — blocking safety gate ║")
message(STATUS "║    legal_compliance — civilian-use checks  ║")
message(STATUS "║    traceability     — requirements trace   ║")
message(STATUS "║    flight_readiness — full assurance gate  ║")
message(STATUS "║    qualification_bundle — audit artifacts  ║")
message(STATUS "║    software_final   — software release gate║")
message(STATUS "║    benchmark_baseline — perf report sync   ║")
message(STATUS "║    benchmark_guard  — perf regression gate ║")
message(STATUS "║    sitl_smoke       — short runtime check  ║")
message(STATUS "║    sitl_soak        — extended runtime test║")
message(STATUS "║    esp32_runtime_guard — WCET/stack check  ║")
message(STATUS "║    esp32_reboot_campaign — reboot campaign ║")
message(STATUS "║    quickstart_10min — one-command local UX ║")
message(STATUS "║    gen              — run autocoder        ║")
message(STATUS "╚════════════════════════════════════════════╝")
message(STATUS "")
