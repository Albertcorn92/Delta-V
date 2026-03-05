cmake_minimum_required(VERSION 3.10)

project(DELTA_V_Framework VERSION 0.1)

# Ensure we use C++20 for Concepts and String View support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. Source Discovery
# While we are currently header-heavy, this allows for future .cpp additions
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 2. Build the Executable
add_executable(flight_software ${SOURCES})

# 3. Header Mapping
# Tells the compiler to look in /src for all your .hpp files
target_include_directories(flight_software PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 4. Compiler Safety Flags
if(NOT MSVC)
    target_compile_options(flight_software PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        -Wno-unused-parameter # Useful for base class overrides
    )
endif()

# 5. Threading Support
# Since we use std::thread and std::mutex in ParamDb and Scheduler
find_package(Threads REQUIRED)
target_link_libraries(flight_software PRIVATE Threads::Threads)