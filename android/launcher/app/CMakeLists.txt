# ==================================================================================================
# 📱 reVC – Mobile Edition (Android) – Root CMake Configuration
#
# Description:
# This file defines the root CMake configuration for the reVC – Mobile Edition,
# targeting the Android platform via the Android NDK. It sets up prebuilt libraries
# (SDL2, OpenAL, mpg123) and then includes the main game CMake build.
# ==================================================================================================

cmake_minimum_required(VERSION 3.22.1 FATAL_ERROR)

message(" ")
message("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(" *  🌐  reVC – Build Configuration Initialized")
message(" *  🔧  Preparing Android platform modules and environment configuration...")
message("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(" ")

# ────────────────────────────────────────────────────────────────────────────────
# 🔖 Project Metadata Setup
# ────────────────────────────────────────────────────────────────────────────────
set(REVC_PROJECT_NAME "reVC")
set(REVC_VERSION 0.0.1)
set(REVC_DESCRIPTION "reverted ${REVC_PROJECT_NAME}")
set(REVC_LIB_NAME revc)

project(${REVC_PROJECT_NAME}
        VERSION ${REVC_VERSION}
        LANGUAGES C CXX
        DESCRIPTION ${REVC_DESCRIPTION}
)

# ────────────────────────────────────────────────────────────────────────────────
# 📂 Define Important Directory Paths
# ────────────────────────────────────────────────────────────────────────────────
get_filename_component(REVC_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(REVC_GAME_DIR "${REVC_ROOT_DIR}/../../../")
message(" *  📂 Game source directory set to: ${REVC_GAME_DIR}")

# ────────────────────────────────────────────────────────────────────────────────
# 📦 Prebuilt Native Libraries for Android
# ────────────────────────────────────────────────────────────────────────────────

# SDL2
add_library(SDL2::SDL2 SHARED IMPORTED)
set_target_properties(SDL2::SDL2 PROPERTIES
    IMPORTED_LOCATION "${REVC_GAME_DIR}/vendor/sdl2/libs/Android/${ANDROID_ABI}/libSDL2.so"
    INTERFACE_INCLUDE_DIRECTORIES "${REVC_GAME_DIR}/vendor/sdl2/include"
)
message(" *  📦 SDL2 prebuilt: ${ANDROID_ABI}")

# OpenAL
add_library(OpenAL::OpenAL SHARED IMPORTED)
set_target_properties(OpenAL::OpenAL PROPERTIES
    IMPORTED_LOCATION "${REVC_GAME_DIR}/vendor/openal-soft/libs/Android/${ANDROID_ABI}/libopenal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${REVC_GAME_DIR}/vendor/openal-soft/include"
)
message(" *  📦 OpenAL prebuilt: ${ANDROID_ABI}")

# mpg123
add_library(MPG123::libmpg123 SHARED IMPORTED)
set_target_properties(MPG123::libmpg123 PROPERTIES
    IMPORTED_LOCATION "${REVC_GAME_DIR}/vendor/mpg123/lib/Android/${ANDROID_ABI}/libmpg123.so"
    INTERFACE_INCLUDE_DIRECTORIES "${REVC_GAME_DIR}/vendor/mpg123/include"
)
message(" *  📦 mpg123 prebuilt: ${ANDROID_ABI}")

# ────────────────────────────────────────────────────────────────────────────────
# ➕ Add Game Subdirectory If Available
# ────────────────────────────────────────────────────────────────────────────────
if(IS_DIRECTORY "${REVC_GAME_DIR}" AND EXISTS "${REVC_GAME_DIR}/CMakeLists.txt")
    message(" *  ➕ Including Game module from: ${REVC_GAME_DIR}")
    message(" ")
    message("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    message(" *  🚀 Initiating Game module build configuration...")
    message("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
    message(" ")

    set(REVC_AUDIO "OAL" CACHE STRING "Audio" FORCE)
    set(REVC_VENDORED_LIBRW ON CACHE BOOL "Use vendored librw" FORCE)
    set(REVC_INSTALL OFF CACHE BOOL "Install" FORCE)

    add_subdirectory(${REVC_GAME_DIR} ${CMAKE_BINARY_DIR}/game_build)

    # Force lowercase output name — System.loadLibrary("revc") looks for librevc.so
    set_target_properties(reVC PROPERTIES LIBRARY_OUTPUT_NAME revc)

else()
    message(" *  ⚠️ Game source directory or CMakeLists.txt not found at: ${REVC_GAME_DIR}")
    message(FATAL_ERROR " *  ❗ Please ensure the game module sources are available for the build.")
endif()