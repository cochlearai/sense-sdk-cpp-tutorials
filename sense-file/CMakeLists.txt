# sense-file: file-mode tutorial. See ../README.md for usage.
cmake_minimum_required(VERSION 3.10)

# ---------------------------------------------------------------------------
# sense-file executable
# ---------------------------------------------------------------------------
add_executable(sense-file sense_file.cc)
target_include_directories(sense-file PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${SENSE_EXTRA_INCLUDE})
target_link_libraries(sense-file PRIVATE ${SENSE_LINK})
target_compile_options(sense-file PRIVATE ${TUTORIAL_COMPILE_OPTIONS})

set_target_properties(sense-file PROPERTIES
  OUTPUT_NAME sense-file-app
  BUILD_RPATH "${SENSE_LIB_DIR}"
  INSTALL_RPATH "${SENSE_LIB_DIR}")

# The binary (sense-file-app) is emitted into the working directory by the parent CMakeLists,
# next to the tracked config.json and audio-files/, so it runs in place.
