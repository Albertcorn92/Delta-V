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
# Target: run_tests  (Google Test suite)
# Enabled by: cmake --build build --target run_tests
# =============================================================================
enable_testing()
find_package(GTest QUIET)

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

    # ── Coverage binary ───────────────────────────────────────────────────────
    add_executable(run_tests_coverage tests/unit_tests.cpp ${ALL_SOURCES})
    target_include_directories(run_tests_coverage PRIVATE src)
    target_link_libraries(run_tests_coverage PRIVATE Threads::Threads GTest::GTest GTest::Main)
    target_compile_options(run_tests_coverage PRIVATE ${DELTAV_COVERAGE_FLAGS})
    target_compile_definitions(run_tests_coverage PRIVATE DELTAV_FAULT_INJECT)
    target_link_options(run_tests_coverage PRIVATE --coverage)

    find_program(LCOV    lcov)
    find_program(GENHTML genhtml)
    set(DELTAV_GCOV_CANDIDATES gcov)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(REGEX MATCH "^[0-9]+" DELTAV_GCC_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
        if(DELTAV_GCC_MAJOR)
            list(PREPEND DELTAV_GCOV_CANDIDATES gcov-${DELTAV_GCC_MAJOR})
        endif()
    endif()
    find_program(DELTAV_GCOV_TOOL NAMES ${DELTAV_GCOV_CANDIDATES})
    set(DELTAV_LCOV_GCOV_ARGS)
    if(DELTAV_GCOV_TOOL)
        list(APPEND DELTAV_LCOV_GCOV_ARGS --gcov-tool ${DELTAV_GCOV_TOOL})
    endif()
    if(LCOV AND GENHTML)
        add_custom_target(coverage
            COMMAND run_tests_coverage --gtest_color=yes
            COMMAND ${LCOV}    ${DELTAV_LCOV_GCOV_ARGS} --capture --directory .
                               --output-file cov_raw.info --quiet
            COMMAND ${LCOV}    ${DELTAV_LCOV_GCOV_ARGS} --remove  cov_raw.info
                               '/usr/*' '*/tests/*' '*/googletest/*'
                               --output-file cov.info --quiet
            COMMAND ${GENHTML} cov.info --output-directory coverage_html --quiet
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
    endif()

    message(STATUS "Tests    : ENABLED  (cmake --build build --target run_tests)")

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

if(TARGET run_tests AND TARGET tidy_safety AND TARGET traceability AND TARGET legal_compliance)
    add_custom_target(flight_readiness
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target legal_compliance
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} --output-on-failure
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
message(STATUS "║    run_tests        — GTest suite          ║")
message(STATUS "║    coverage         — lcov HTML report     ║")
message(STATUS "║    tidy             — clang-tidy analysis  ║")
message(STATUS "║    tidy_safety      — blocking safety gate ║")
message(STATUS "║    legal_compliance — civilian-use checks  ║")
message(STATUS "║    traceability     — requirements trace   ║")
message(STATUS "║    flight_readiness — full assurance gate  ║")
message(STATUS "║    qualification_bundle — audit artifacts  ║")
message(STATUS "║    software_final   — software release gate║")
message(STATUS "║    gen              — run autocoder        ║")
message(STATUS "╚════════════════════════════════════════════╝")
message(STATUS "")
