# Copyright (c) 2026 Synaptics Incorporated
# dlsdkConfig.cmake  --  relocatable package configuration for the hand-assembled,
# multiplatform DisplayLink Direct SDK.
#
# Expected layout (this file sits at the package root):
#
#   dlsdk/
#     dlsdkConfig.cmake            <-- this file
#     dlsdkConfigVersion.cmake
#     include/dlsdk/...            <-- public headers, included as <dlsdk/...>
#     libs/linux/x86_64/libdlsdk.so
#     libs/linux/aarch64/libdlsdk.so
#     libs/windows/x86_64/dlsdk.lib + dlsdk.dll
#     libs/macos/libdlsdk.dylib
#     libs/android/<platform>/<abi>/libdlsdk.so    (e.g. android-21/arm64-v8a)
#
# Use the package with:
#     find_package(dlsdk REQUIRED)          # -DCMAKE_PREFIX_PATH=/path/to/dlsdk
#     target_link_libraries(app PRIVATE dlsdk::dlsdk)

include(CMakeFindDependencyMacro)
find_dependency(Threads)

# The DisplayLink Direct root is the directory containing this configuration file.
get_filename_component(_dlsdk_root "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# ---------------------------------------------------------------------------
# Select the correct platform- and architecture-specific library subdirectory.
# ---------------------------------------------------------------------------
if(WIN32)
    set(_dlsdk_libsubdir "windows/x86_64")
    set(_dlsdk_dll  "${_dlsdk_root}/libs/${_dlsdk_libsubdir}/dlsdk.dll")
    set(_dlsdk_impl "${_dlsdk_root}/libs/${_dlsdk_libsubdir}/dlsdk.lib")
elseif(APPLE)
    set(_dlsdk_libsubdir "macos")
    set(_dlsdk_lib "${_dlsdk_root}/libs/${_dlsdk_libsubdir}/libdlsdk.dylib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Normalise the architecture name supplied by the consuming toolchain.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_dlsdk_libsubdir "linux/aarch64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(_dlsdk_libsubdir "linux/x86_64")
    else()
        set(dlsdk_FOUND FALSE)
        set(dlsdk_NOT_FOUND_MESSAGE
            "DisplayLink Direct: unsupported Linux architecture '${CMAKE_SYSTEM_PROCESSOR}'.")
        return()
    endif()
    set(_dlsdk_lib "${_dlsdk_root}/libs/${_dlsdk_libsubdir}/libdlsdk.so")
elseif(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Use the Android platform (API level) and ABI supplied by the toolchain.
    if(NOT ANDROID_PLATFORM)
        set(dlsdk_FOUND FALSE)
        set(dlsdk_NOT_FOUND_MESSAGE
            "DisplayLink Direct: Android platform not detected. Pass "
            "-DANDROID_PLATFORM=<android-NN> (e.g. android-21).")
        return()
    endif()
    if(NOT ANDROID_ABI)
        set(dlsdk_FOUND FALSE)
        set(dlsdk_NOT_FOUND_MESSAGE
            "DisplayLink Direct: Android ABI not detected. Pass "
            "-DANDROID_ABI=<armeabi-v7a|arm64-v8a|x86|x86_64>.")
        return()
    endif()
    set(_dlsdk_libsubdir "${ANDROID_PLATFORM}/${ANDROID_ABI}")
    set(_dlsdk_lib "${_dlsdk_root}/libs/${_dlsdk_libsubdir}/libdlsdk.so")
else()
    set(dlsdk_FOUND FALSE)
    set(dlsdk_NOT_FOUND_MESSAGE
        "DisplayLink Direct: unsupported platform '${CMAKE_SYSTEM_NAME}'.")
    return()
endif()

# ---------------------------------------------------------------------------
# Verify that the artifacts are present for this platform.
# ---------------------------------------------------------------------------
set(_dlsdk_includedir "${_dlsdk_root}/include")

if(WIN32)
    if(NOT EXISTS "${_dlsdk_dll}" OR NOT EXISTS "${_dlsdk_impl}")
        set(dlsdk_FOUND FALSE)
        set(dlsdk_NOT_FOUND_MESSAGE
            "DisplayLink Direct: missing library files under libs/${_dlsdk_libsubdir} "
            "(expected dlsdk.dll and dlsdk.lib).")
        return()
    endif()
else()
    if(NOT EXISTS "${_dlsdk_lib}")
        set(dlsdk_FOUND FALSE)
        set(dlsdk_NOT_FOUND_MESSAGE
            "DisplayLink Direct: missing library file '${_dlsdk_lib}'.")
        return()
    endif()
endif()

# ---------------------------------------------------------------------------
# Define the imported target.
# ---------------------------------------------------------------------------
if(NOT TARGET dlsdk::dlsdk)
    add_library(dlsdk::dlsdk SHARED IMPORTED)
    set_target_properties(dlsdk::dlsdk PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_dlsdk_includedir}"
        INTERFACE_LINK_LIBRARIES "Threads::Threads"
    )
    if(WIN32)
        set_target_properties(dlsdk::dlsdk PROPERTIES
            IMPORTED_LOCATION "${_dlsdk_dll}"
            IMPORTED_IMPLIB "${_dlsdk_impl}"
        )
    else()
        set_target_properties(dlsdk::dlsdk PROPERTIES
            IMPORTED_LOCATION "${_dlsdk_lib}"
        )
    endif()
endif()

unset(_dlsdk_root)
unset(_dlsdk_libsubdir)
unset(_dlsdk_includedir)
unset(_dlsdk_lib)
unset(_dlsdk_dll)
unset(_dlsdk_impl)
