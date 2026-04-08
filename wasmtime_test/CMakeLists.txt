cmake_minimum_required(VERSION 3.16)
project(wasmtime_runner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()

set(WASMTIME_ROOT "$ENV{HOME}/.wasmtime/wasmtime-dev-aarch64-linux-c-api/" CACHE PATH "Wasmtime C API path")

find_path(WASMTIME_INCLUDE_DIR
    NAMES wasmtime.h
    PATHS ${WASMTIME_ROOT}/include
)

find_library(WASMTIME_LIBRARY
    NAMES wasmtime
    PATHS ${WASMTIME_ROOT}/lib
)

if(NOT WASMTIME_INCLUDE_DIR OR NOT WASMTIME_LIBRARY)
    message(FATAL_ERROR
        "Wasmtime C API not found. Please set WASMTIME_ROOT to the correct path"
        " where wasmtime.h and libwasmtime.so are located.")
endif()

add_executable(wasmtime_runner src/wasmtime_runner.cpp)
set_target_properties(wasmtime_runner PROPERTIES
    OUTPUT_NAME wasmtime_runner
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON)

target_include_directories(wasmtime_runner PRIVATE ${WASMTIME_INCLUDE_DIR})
target_link_libraries(wasmtime_runner PRIVATE ${WASMTIME_LIBRARY})

target_compile_options(wasmtime_runner PRIVATE -Wall -Wextra -Wno-deprecated-declarations)