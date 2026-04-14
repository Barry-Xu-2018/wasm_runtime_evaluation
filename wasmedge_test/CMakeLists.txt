cmake_minimum_required(VERSION 3.16)
project(wasmedge_runner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT DEFINED WASMEDGE_ROOT)
    if(DEFINED ENV{WASMEDGE_ROOT})
        set(WASMEDGE_ROOT "$ENV{WASMEDGE_ROOT}" CACHE PATH "WasmEdge installation root directory")
        message(STATUS "Read WASMEDGE_ROOT from environment variable: ${WASMEDGE_ROOT}")
    else()
        message(FATAL_ERROR "Error: WASMEDGE_ROOT not configured!\n")
    endif()
endif()

if(NOT EXISTS "${WASMEDGE_ROOT}")
    message(FATAL_ERROR
        "========================================\n"
        "  Error: WASMEDGE_ROOT does not exist!\n"
        "========================================\n"
        "  Current path: ${WASMEDGE_ROOT}\n"
        "========================================\n")
endif()

message(STATUS "WASMEDGE_ROOT = ${WASMEDGE_ROOT}")

find_path(WASMEDGE_INCLUDE_DIR
  NAMES wasmedge/wasmedge.h
  PATHS ${WASMEDGE_ROOT}/include)

find_library(WASMEDGE_LIBRARY
  NAMES wasmedge
  PATHS ${WASMEDGE_ROOT}/lib)

if(NOT WASMEDGE_INCLUDE_DIR OR NOT WASMEDGE_LIBRARY)
  message(FATAL_ERROR "WasmEdge not found!\n")
endif()

function(add_wasmedge_executable TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    target_include_directories(${TARGET_NAME} PRIVATE ${WASMEDGE_INCLUDE_DIR})
    target_link_libraries(${TARGET_NAME} PRIVATE ${WASMEDGE_LIBRARY})
    target_compile_options(
      ${TARGET_NAME} PRIVATE
      -Wall
      -Wextra)
endfunction()

add_wasmedge_executable(wasmedge_runner src/wasmedge_runner.cpp)
add_wasmedge_executable(wasmedge_aot_runner src/wasmedge_aot_runner.cpp)

