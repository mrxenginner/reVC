# ==================================================================================================
# Android vendor library wiring
#
# The desktop builds (Windows/Linux/macOS) get SDL2/OpenAL/mpg123 through Conan,
# which populates the normal find_package() search paths automatically. Android
# has no Conan profile here, so this file points the very same find_package()
# calls used by src/CMakeLists.txt at the prebuilt, per-ABI binaries already
# vendored under vendor/<lib>/.../Android/${ANDROID_ABI}/.
#
# Included from android/launcher/app/CMakeLists.txt, before add_subdirectory()
# pulls in the game module.
# ==================================================================================================

if(NOT ANDROID)
    return()
endif()

if(NOT DEFINED ANDROID_ABI OR ANDROID_ABI STREQUAL "")
    message(FATAL_ERROR "AndroidConfig.cmake: ANDROID_ABI is not set")
endif()

get_filename_component(REVC_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/../../vendor" ABSOLUTE)

message(STATUS "AndroidConfig: wiring vendor libraries for ABI ${ANDROID_ABI} from ${REVC_VENDOR_DIR}")

# --- SDL2 (consumed by vendor/librw's own FindSDL2.cmake) -----------------------------------
set(SDL2_INCLUDE_DIR "${REVC_VENDOR_DIR}/sdl2/include" CACHE PATH "" FORCE)
set(SDL2_LIBRARY "${REVC_VENDOR_DIR}/sdl2/libs/Android/${ANDROID_ABI}/libSDL2.so" CACHE FILEPATH "" FORCE)

# --- OpenAL (consumed by CMake's builtin FindOpenAL.cmake) ----------------------------------
set(OPENAL_INCLUDE_DIR "${REVC_VENDOR_DIR}/openal-soft/include" CACHE PATH "" FORCE)
set(OPENAL_LIBRARY "${REVC_VENDOR_DIR}/openal-soft/libs/Android/${ANDROID_ABI}/libopenal.so" CACHE FILEPATH "" FORCE)

# --- mpg123 (consumed by cmake/Findmpg123.cmake) ---------------------------------------------
set(mpg123_INCLUDE_DIR "${REVC_VENDOR_DIR}/mpg123/include" CACHE PATH "" FORCE)
set(mpg123_LIBRARIES "${REVC_VENDOR_DIR}/mpg123/lib/Android/${ANDROID_ABI}/libmpg123.so" CACHE FILEPATH "" FORCE)

foreach(_revc_check IN ITEMS SDL2_LIBRARY OPENAL_LIBRARY mpg123_LIBRARIES)
    if(NOT EXISTS "${${_revc_check}}")
        message(FATAL_ERROR "AndroidConfig: expected prebuilt library not found: ${${_revc_check}}")
    endif()
endforeach()
