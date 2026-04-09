cmake_minimum_required(VERSION 3.10)
project(benchmark_wasm LANGUAGES CXX)

if(NOT DEFINED WASI_SDK_PATH)
    if(DEFINED ENV{WASI_SDK_PATH})
        set(WASI_SDK_PATH "$ENV{WASI_SDK_PATH}" CACHE PATH "WASI SDK installation path")
        message(STATUS "Read WASI_SDK_PATH from environment variable: ${WASI_SDK_PATH}")
    else()
        message(FATAL_ERROR
            "========================================\n"
            "  Error: WASI_SDK_PATH not configured!\n"
            "========================================\n"
            "Please configure WASI_SDK_PATH using one of the following methods:\n"
            "  Method 1 - Set environment variable:\n"
            "    export WASI_SDK_PATH=/opt/wasi-sdk\n"
            "  Method 2 - CMake command line argument:\n"
            "    cmake -DWASI_SDK_PATH=/opt/wasi-sdk ..\n"
            "========================================\n"
        )
    endif()
endif()

if(NOT EXISTS "${WASI_SDK_PATH}")
    message(FATAL_ERROR
        "========================================\n"
        "  Error: WASI_SDK_PATH does not exist!\n"
        "========================================\n"
        "  Current path: ${WASI_SDK_PATH}\n"
        "  Please ensure WASI SDK is correctly installed.\n"
        "  Download link: https://github.com/WebAssembly/wasi-sdk/releases\n"
        "========================================\n"
    )
endif()

message(STATUS "WASI_SDK_PATH = ${WASI_SDK_PATH}")

set(CMAKE_CXX_COMPILER "${WASI_SDK_PATH}/bin/clang++")
set(CMAKE_SYSROOT "${WASI_SDK_PATH}/share/wasi-sysroot")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -target wasm32-wasip1")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fno-exceptions -fno-rtti")

add_executable(benchmark_wasm src/benchmark.cpp)

set_target_properties(benchmark_wasm PROPERTIES
    OUTPUT_NAME benchmark
    SUFFIX ".wasm"
)
