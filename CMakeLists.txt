cmake_minimum_required(VERSION 3.10)
project(DELTA_V_Framework VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. Threading Support
find_package(Threads REQUIRED)

# 2. Flight Software Executable
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(flight_software ${SOURCES})
target_include_directories(flight_software PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(flight_software PRIVATE Threads::Threads)

# 3. Compiler Safety Flags
if(NOT MSVC)
    target_compile_options(flight_software PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# 4. Unit Testing (Google Test)
# This section allows 'make' to build a separate testing binary
enable_testing()
find_package(GTest REQUIRED)

add_executable(run_tests tests/unit_tests.cpp)
target_include_directories(run_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(run_tests PRIVATE Threads::Threads GTest::GTest GTest::Main)