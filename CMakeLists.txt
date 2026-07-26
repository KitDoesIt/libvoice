cmake_minimum_required(VERSION 3.16)
project(godot_voice VERSION 0.1.0 LANGUAGES C)

# --- Feature detection ------------------------------------------------
# Check if OPUS_SET_DRED_DURATION exists (libopus >= 1.5)
include(CheckSymbolExists)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_SOURCE_DIR}/extern/opus/include)
check_symbol_exists(OPUS_SET_DRED_DURATION_REQUEST "opus/opus_defines.h" OPUS_HAS_DRED)
unset(CMAKE_REQUIRED_INCLUDES)

# Silence the "_FORTIFY_SOURCE requires optimization" warning spam
include(CheckCCompilerFlag)
check_c_compiler_flag("-Wno-unused-command-line-argument" CC_HAVE_NO_UNUSED_CLI)
if(CC_HAVE_NO_UNUSED_CLI)
  set_source_files_properties(
    extern/rnnoise/src/celt_lpc.c
    extern/rnnoise/src/denoise.c
    extern/rnnoise/src/kiss_fft.c
    extern/rnnoise/src/nnet_default.c
    extern/rnnoise/src/pitch.c
    extern/rnnoise/src/rnn.c
    extern/rnnoise/src/rnn_data.c
    extern/rnnoise/src/vec.c
    extern/opus/src/mlp.c
    extern/opus/src/opus.c
    extern/opus/src/opus_decoder.c
    extern/opus/src/opus_encoder.c
    extern/opus/src/opus_multistream.c
    extern/opus/src/opus_projection_decoder.c
    extern/opus/src/opus_projection_encoder.c
    extern/opus/src/repacketizer.c
    PROPERTIES COMPILE_FLAGS "-Wno-unused-command-line-argument;-Wno-deprecated-non-prototype;-Wno-unknown-warning-option"
  )
endif()

# Fix: avoid multiple includes of opus/opus_types.h
include_directories(extern/opus/include)
include_directories(extern/opus)
include_directories(extern/rnnoise/include)
include_directories(extern/rnnoise/src)
include_directories(include)

# --- Internal helpers ------------------------------------------------
# On macOS/Linux we want shared libs; on Windows we build static for simplicity.
# This simplifies the C# P/Invoke story (no DLL hell).
option(OPUS_BUILD_SHARED_LIBS "Build opus as shared library" OFF)

# Silence -Wshorten-64-to-32 only for this project
# (opus and rnnoise are fine, but our thin wrappers may sign-convert)
if(APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shorten-64-to-32")
endif()

# --------------------------------------------------------------------
#  Extern: Opus
# --------------------------------------------------------------------
set(OPUS_SOURCES
  extern/opus/src/mlp.c
  extern/opus/src/opus.c
  extern/opus/src/opus_decoder.c
  extern/opus/src/opus_encoder.c
  extern/opus/src/opus_multistream.c
  extern/opus/src/opus_projection_decoder.c
  extern/opus/src/opus_projection_encoder.c
  extern/opus/src/repacketizer.c
)

add_library(opus_lib STATIC ${OPUS_SOURCES})
target_include_directories(opus_lib PUBLIC
  extern/opus/include
  extern/opus
)
target_compile_definitions(opus_lib PRIVATE
  OPUS_BUILD
  VAR_ARRAYS
  DISABLE_FLOAT_API
  FLOATING_POINT
)
# On non-Windows, use visibility for exported symbols
if(NOT WIN32)
  target_compile_definitions(opus_lib PRIVATE HAVE_CONFIG_H=1)
endif()

# --------------------------------------------------------------------
#  Extern: RNNoise
# --------------------------------------------------------------------
set(RNNOISE_SOURCES
  extern/rnnoise/src/celt_lpc.c
  extern/rnnoise/src/denoise.c
  extern/rnnoise/src/kiss_fft.c
  extern/rnnoise/src/nnet_default.c
  extern/rnnoise/src/pitch.c
  extern/rnnoise/src/rnn.c
  extern/rnnoise/src/rnn_data.c
  extern/rnnoise/src/vec.c
)

add_library(rnnoise_lib STATIC ${RNNOISE_SOURCES})
target_include_directories(rnnoise_lib PUBLIC
  extern/rnnoise/include
  extern/rnnoise/src
)
target_compile_definitions(rnnoise_lib PRIVATE
  RNNOISE_BUILD
)

# --------------------------------------------------------------------
#  Voice Codec library (shared / dynamic)
# --------------------------------------------------------------------
set(VOICE_CODEC_SOURCES
  src/voice_codec.c
  src/voice_encoder.c
  src/voice_decoder.c
)

add_library(voice_codec SHARED ${VOICE_CODEC_SOURCES})
target_link_libraries(voice_codec PRIVATE opus_lib rnnoise_lib m)
# Export symbols on all platforms
set_target_properties(voice_codec PROPERTIES
  C_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(voice_codec PRIVATE -fvisibility=hidden)
  # Re-export the public API
  set_target_properties(voice_codec PROPERTIES
    LINK_FLAGS "-fvisibility=hidden"
  )
endif()
target_compile_definitions(voice_codec PRIVATE VOICE_CODEC_BUILD)

# On Windows, mark the public functions as dllexport
if(WIN32)
  target_compile_definitions(voice_codec PRIVATE VOICE_CODEC_EXPORTS)
endif()

# --------------------------------------------------------------------
#  Install
# --------------------------------------------------------------------
install(TARGETS voice_codec LIBRARY DESTINATION lib)
install(FILES include/voice_codec.h DESTINATION include)
