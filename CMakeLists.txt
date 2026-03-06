cmake_minimum_required(VERSION 3.15)
project(DELTA_V_Framework VERSION 2.0 LANGUAGES CXX)

# =============================================================================
# Global Settings
# =============================================================================
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)  # No GNU extensions — strict ISO C++20

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

# =============================================================================
# Compiler Safety Flags
# =============================================================================
set(DELTAV_WARN_FLAGS
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wconversion          # Warn on implicit narrowing conversions
    -Wshadow              # Warn when local variable shadows outer scope
    -Wnull-dereference    # Warn on potential null dereference paths
    -Wformat=2            # Strict printf/scanf format checking
)

# Release: optimize for embedded targets
set(DELTAV_RELEASE_FLAGS -O2 -DNDEBUG)

# Debug + Sanitizers: catch UB and memory errors during development
set(DELTAV_SANITIZER_FLAGS
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -g
)

# Coverage: for CI coverage report
set(DELTAV_COVERAGE_FLAGS
    --coverage
    -fprofile-arcs
    -ftest-coverage
    -O0 -g
)

# =============================================================================
# Build Type Presets
# =============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# Flight Software Executable
# =============================================================================
file(GLOB_RECURSE FLIGHT_SOURCES "src/*.cpp")

add_executable(flight_software ${FLIGHT_SOURCES})
target_include_directories(flight_software PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(flight_software PRIVATE Threads::Threads)
target_compile_options(flight_software PRIVATE ${DELTAV_WARN_FLAGS})

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(flight_software PRIVATE ${DELTAV_RELEASE_FLAGS})
endif()

# =============================================================================
# Unit Test Executable
# =============================================================================
enable_testing()

# Find GTest — install with: sudo apt install libgtest-dev (Ubuntu)
#                             brew install googletest       (macOS)
find_package(GTest QUIET)

if(GTest_FOUND OR GTEST_FOUND)
    add_executable(run_tests tests/unit_tests.cpp)
    target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(run_tests PRIVATE Threads::Threads GTest::GTest GTest::Main)
    target_compile_options(run_tests PRIVATE ${DELTAV_WARN_FLAGS})

    # Sanitizer build (default for test target in Debug mode)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT APPLE)
        # ASan + UBSan — catches buffer overflows, use-after-free, UB
        target_compile_options(run_tests PRIVATE ${DELTAV_SANITIZER_FLAGS})
        target_link_options(run_tests PRIVATE -fsanitize=address,undefined)
    endif()

    # Register with CTest
    add_test(NAME DeltaV_Unit_Tests COMMAND run_tests)
    set_tests_properties(DeltaV_Unit_Tests PROPERTIES TIMEOUT 30)

    message(STATUS "Unit tests: ENABLED (target: run_tests)")

    # Coverage target (builds separate binary with --coverage flags)
    add_executable(run_tests_coverage tests/unit_tests.cpp)
    target_include_directories(run_tests_coverage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(run_tests_coverage PRIVATE Threads::Threads GTest::GTest GTest::Main)
    target_compile_options(run_tests_coverage PRIVATE ${DELTAV_COVERAGE_FLAGS})
    target_link_options(run_tests_coverage PRIVATE --coverage)

    # Custom target: make coverage — runs tests and generates lcov HTML report
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        add_custom_target(coverage
            COMMAND run_tests_coverage
            COMMAND ${LCOV_EXECUTABLE} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage_filtered.info
            COMMAND ${GENHTML_EXECUTABLE} coverage_filtered.info --output-directory coverage_report
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: coverage_report/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report"
            DEPENDS run_tests_coverage
        )
    endif()
else()
    message(WARNING "GTest not found. Unit tests disabled. Install: sudo apt install libgtest-dev")
endif()

# =============================================================================
# Static Analysis Target (clang-tidy)
# Usage: make tidy
# =============================================================================
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
if(CLANG_TIDY_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES "src/*.hpp" "src/*.cpp")
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            -p ${CMAKE_BINARY_DIR}
            --checks=cppcoreguidelines-*,cert-*,performance-*,modernize-*,bugprone-*
            --warnings-as-errors=*
            ${ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy static analysis"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    message(STATUS "Static analysis: ENABLED (target: tidy)")
else()
    message(STATUS "clang-tidy not found. Install: sudo apt install clang-tidy")
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== DELTA-V Build Configuration ===")
message(STATUS "  CXX Standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")