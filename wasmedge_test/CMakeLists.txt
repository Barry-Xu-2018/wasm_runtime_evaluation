cmake_minimum_required(VERSION 3.16)
project(wasmedge_runner CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(WASMEDGE_ROOT "$ENV{HOME}/.wasmedge" CACHE PATH "WasmEdge installation root directory")

find_path(WASMEDGE_INCLUDE_DIR
  NAMES wasmedge/wasmedge.h
  PATHS ${WASMEDGE_ROOT}/include)

find_library(WASMEDGE_LIBRARY
  NAMES wasmedge
  PATHS ${WASMEDGE_ROOT}/lib)

if(NOT WASMEDGE_INCLUDE_DIR OR NOT WASMEDGE_LIBRARY)
  message(FATAL_ERROR "WasmEdge not found!\n")
endif()

add_executable(wasmedge_runner src/wasmedge_runner.cpp)

target_include_directories(
  wasmedge_runner PRIVATE
  ${WASMEDGE_INCLUDE_DIR})

target_link_libraries(
  wasmedge_runner PRIVATE
  ${WASMEDGE_LIBRARY})

target_compile_options(
  wasmedge_runner PRIVATE
  -Wall
  -Wextra)
