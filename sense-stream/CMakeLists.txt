# sense-stream: stream-mode tutorial (real-time microphone capture).
cmake_minimum_required(VERSION 3.10)

# ---------------------------------------------------------------------------
# miniaudio: vendored single-header capture library (sense-stream only). The
# sense SDK has no audio-device dependency; capture is entirely tutorial-side.
# Its implementation lives in its own C TU, marked SYSTEM so -Werror
# -pedantic does not apply to the third-party header. At runtime it uses
# ALSA/PulseAudio (Linux) or CoreAudio (macOS); nothing extra to build.
# ---------------------------------------------------------------------------
add_library(miniaudio STATIC third_party/miniaudio_impl.c)
target_include_directories(miniaudio SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(APPLE)
  target_link_libraries(miniaudio PUBLIC
    "-framework CoreFoundation"
    "-framework CoreAudio"
    "-framework AudioToolbox")
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(miniaudio PUBLIC ${CMAKE_DL_LIBS} Threads::Threads m)
endif()

# ---------------------------------------------------------------------------
# sense-stream executable
# ---------------------------------------------------------------------------
add_executable(sense-stream sense_stream.cc microphone.cc)
target_include_directories(sense-stream PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${SENSE_EXTRA_INCLUDE})
target_link_libraries(sense-stream PRIVATE ${SENSE_LINK} miniaudio)
target_compile_options(sense-stream PRIVATE ${TUTORIAL_COMPILE_OPTIONS})

set_target_properties(sense-stream PROPERTIES
  OUTPUT_NAME sense-stream-app
  BUILD_RPATH "${SENSE_LIB_DIR}"
  INSTALL_RPATH "${SENSE_LIB_DIR}")

# The binary (sense-stream-app) is emitted into the working directory by the parent
# CMakeLists, next to the tracked config.json, so it runs in place.
