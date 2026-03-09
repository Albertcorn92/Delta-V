set(DELTAV_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

option(DELTAV_ALLOW_IMU_SIMULATION "Allow IMU simulation fallback when I2C sensor is missing" ON)
option(DELTAV_LOCAL_ONLY "Disable network bridge for strict local-only runtime" ON)
option(DELTAV_DISABLE_NETWORK_STACK "Disable ESP-IDF network stack dependencies for compliance profile" ON)
option(DELTAV_ESP_SAFE_MODE "Run ESP target in stable local-only safe mode loop" OFF)

set(DELTAV_REQUIRES
    driver
    esp_timer
    freertos
    nvs_flash
)

if(NOT DELTAV_DISABLE_NETWORK_STACK)
    list(APPEND DELTAV_REQUIRES lwip)
endif()

file(GLOB DELTAV_SRC
    ${DELTAV_ROOT}/src/*.cpp
)

idf_component_register(
    SRCS
        app_main.cpp
        ${DELTAV_SRC}
    INCLUDE_DIRS
        ${DELTAV_ROOT}/src
    REQUIRES
        ${DELTAV_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -fexceptions)
target_compile_definitions(${COMPONENT_LIB} PRIVATE DELTAV_OS_FREERTOS)

if(DELTAV_ALLOW_IMU_SIMULATION)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DELTAV_ALLOW_IMU_SIMULATION)
endif()

if(DELTAV_LOCAL_ONLY)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DELTAV_LOCAL_ONLY)
endif()

if(DELTAV_DISABLE_NETWORK_STACK)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DELTAV_DISABLE_NETWORK_STACK)
endif()

if(DELTAV_ESP_SAFE_MODE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DELTAV_ESP_SAFE_MODE)
endif()
