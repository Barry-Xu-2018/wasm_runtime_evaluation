cmake_minimum_required(VERSION 3.10)
project(benchmark_wasm LANGUAGES CXX)

set(WASI_SDK_PATH "/opt/wasi-sdk-32.0-arm64-linux" CACHE STRING "Path to WASI SDK")

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
