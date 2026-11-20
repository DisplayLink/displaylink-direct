# Copyright (c) 2026 Synaptics Incorporated
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22.1)

project(dlvideo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The Android NDK toolchain sets CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY, which confines
# find_package() to the NDK sysroot and ignores CMAKE_PREFIX_PATH. DisplayLink Direct is a
# host-side SDK bundle outside the sysroot, so allow package lookup in both locations.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# dlsdkConfig.cmake selects libs/<ANDROID_PLATFORM>/<ANDROID_ABI>/libdlsdk.so, and DisplayLink
# Direct publishes its Android binaries under android-21. The application itself is compiled
# against a much newer API level, so ANDROID_PLATFORM is only overridden for the package lookup.
# The toolchain has already configured the compiler at this point, so restoring it afterwards
# keeps the real target API intact.
set(DLSDK_ANDROID_PLATFORM "android-21" CACHE STRING
    "Platform directory holding the DisplayLink Direct Android binaries")

set(_application_android_platform "${ANDROID_PLATFORM}")
set(ANDROID_PLATFORM "${DLSDK_ANDROID_PLATFORM}")
find_package(dlsdk 2.1.0 REQUIRED)
set(ANDROID_PLATFORM "${_application_android_platform}")

add_library(dlvideo SHARED
    jni_bridge.cpp
    dlsdk_session.cpp
    gl_compositor.cpp
    video_engine.cpp
)

target_compile_options(dlvideo PRIVATE -Wall -Wextra -O3)

target_link_libraries(dlvideo PRIVATE
    dlsdk::dlsdk
    android      # ASurfaceTexture
    nativewindow # AHardwareBuffer
    EGL
    GLESv3
    log
)
