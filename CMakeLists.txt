# sense-sdk-cpp-tutorials
#
# Prerequisites: a C++14 compiler and CMake >= 3.10. sense-stream captures the
# microphone via the vendored miniaudio header (sense-stream/third_party/);
# the SDK library itself has no audio-device dependency

cmake_minimum_required(VERSION 3.10)

# C is required for the miniaudio implementation TU (sense-stream/third_party/*.c).
project(sense-tutorials C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Locate the sense SDK: a directory containing include/ (public headers) and
# lib/ (libsense-core). Defaults to ./sense next to this file -- override only
# if your SDK lives elsewhere.
# ---------------------------------------------------------------------------
set(SENSE_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sense" CACHE PATH
  "Directory containing the sense SDK (include/ + lib/)")

find_library(SENSE_CORE_LIBRARY
  NAMES sense-core
  PATHS "${SENSE_SDK_ROOT}/lib"
  NO_DEFAULT_PATH)
if(NOT SENSE_CORE_LIBRARY)
  message(FATAL_ERROR "libsense-core not found under ${SENSE_SDK_ROOT}/lib.")
endif()
if(NOT EXISTS "${SENSE_SDK_ROOT}/include/sense/sense.hpp")
  message(FATAL_ERROR
    "sense/sense.hpp not found under ${SENSE_SDK_ROOT}/include.")
endif()

set(SENSE_LINK "${SENSE_CORE_LIBRARY}")
set(SENSE_EXTRA_INCLUDE "${SENSE_SDK_ROOT}/include")
set(SENSE_LIB_DIR "${SENSE_SDK_ROOT}/lib")

set(TUTORIAL_COMPILE_OPTIONS -Wall -Wextra -Werror -pedantic)

# Emit binaries into this dir, next to config.json/audio-files/, so tutorials
# run in place; each target gets an "-app" OUTPUT_NAME suffix (own CMakeLists)
# to avoid a name clash with its source subdirectory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(sense-file)
add_subdirectory(sense-stream)
