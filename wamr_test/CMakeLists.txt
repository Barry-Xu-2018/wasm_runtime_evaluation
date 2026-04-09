cmake_minimum_required(VERSION 3.16)
project(wamr_runner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()

if(NOT DEFINED WAMR_ROOT)
    if(DEFINED ENV{WAMR_ROOT})
        set(WAMR_ROOT "$ENV{WAMR_ROOT}" CACHE PATH "WAMR installation root directory")
        message(STATUS "Read WAMR_ROOT from environment variable: ${WAMR_ROOT}")
    else()
        message(FATAL_ERROR "Error: WAMR_ROOT not configured!\n")
    endif()
endif()

if(NOT EXISTS "${WAMR_ROOT}")
    message(FATAL_ERROR
        "========================================\n"
        "  Error: WAMR_ROOT does not exist!\n"
        "========================================\n"
        "  Current path: ${WAMR_ROOT}\n"
        "========================================\n")
endif()

message(STATUS "WAMR_ROOT = ${WAMR_ROOT}")

find_path(WAMR_INCLUDE_DIR
  NAMES wasm_c_api.h
  PATHS ${WAMR_ROOT}/include)

find_library(WAMR_LIBRARY
    NAMES iwasm
    PATHS ${WAMR_ROOT}/lib)

if(NOT WAMR_INCLUDE_DIR OR NOT WAMR_LIBRARY)
  message(FATAL_ERROR
    "WAMR not found. Please set WAMR_ROOT to the installation directory of WAMR.")
endif()

add_executable(wamr_runner src/wamr_runner.cpp)

target_include_directories(
  wamr_runner PRIVATE
  ${WAMR_INCLUDE_DIR}
)
target_link_libraries(
  wamr_runner PRIVATE
  ${WAMR_LIBRARY})

target_compile_options(wamr_runner PRIVATE -Wall -Wextra)
