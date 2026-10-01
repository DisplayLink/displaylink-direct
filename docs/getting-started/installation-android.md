# Running on Android

## Supported target and bundle assumptions

DisplayLink Direct supports the following Android platforms:

- **ABI**: `arm64-v8a`
- **API level**: `android-21`

## Modes of operation

DisplayLink Direct on Android can be integrated using two modes.

!!! note
    The difference between rooted and unrooted Android USB integration modes is described in the libusb Android README:
    [https://github.com/libusb/libusb/tree/master/android#readme](https://github.com/libusb/libusb/tree/master/android#readme)

### Rooted device mode

This mode uses native USB access directly from `libusb` on device nodes.

- Requires a root shell (`adb root`) or read/write access to the USB device.

### Unrooted device mode

!!! warning
    Unrooted device mode is currently not supported.
    Please contact [support](../support/index.md) for guidance on this
    integration.

This mode requires integrating `libusb` with an Android USB native handle provided by Java `UsbManager`.

- Requires app-side Java/Kotlin USB permission and device-handle handoff to native code
- Requires passing the device handle to DisplayLink Direct, which is not
  currently available.

## Prerequisites (rooted mode)

- Android SDK platform tools (`adb`)
- Rooted Android target device
- libusb >= 1.0.27

    Build `libusb-1.0.so` for Android from the upstream `libusb` repository:

    ```bash
    export NDK=/opt/android/ndk/27.3.13750724
    git clone https://github.com/libusb/libusb.git
    cd libusb/android/jni
    "${NDK}"/ndk-build USE_PC_NAME=1 APP_ALLOW_MISSING_DEPS=true APP_MODULES="usb-1.0" APP_CFLAGS="-DANDROID_OS"

    # build is in libusb/android/libs/arm64-v8a/libusb-1.0.so
    file ../libs/arm64-v8a/libusb-1.0.so
    ```

## Building with CMake (rooted mode)

Build the minimal C example from a clean build directory:

```bash
# CMAKE_PREFIX_PATH should point to dlsdk directory with libs/, firmware/, include/
# e.g. to root of this repository
cd samples/minimal-c
export DLSDK_PATH="$(realpath ../../)"
export NDK=/opt/android/ndk/27.3.13750724

rm -rf build
cmake -DCMAKE_PREFIX_PATH="${DLSDK_PATH}" \
      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
      -DCMAKE_TOOLCHAIN_FILE="${NDK}/build/cmake/android.toolchain.cmake" \
      -DANDROID_PLATFORM=android-21 \
      -DANDROID_ABI=arm64-v8a \
      -S . -B build
cmake --build build
```

!!! warning
    The Android NDK toolchain defaults `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY`.
    Setting it to `BOTH` is required so `find_package(dlsdk)` can resolve the
    host-side package path from `CMAKE_PREFIX_PATH`.

## Deploying and running (rooted mode)

```bash
export LIBUSB_PATH=~/libusb
adb root
adb shell rm -rf /data/local/tmp/minimal-c
adb shell mkdir /data/local/tmp/minimal-c

adb push "${DLSDK_PATH}"/libs/android-21/arm64-v8a/libdlsdk.so   /data/local/tmp/minimal-c
adb push "${LIBUSB_PATH}/android/libs/arm64-v8a/libusb-1.0.so" /data/local/tmp/minimal-c
adb push build/dlsdk-minimal-c-sample /data/local/tmp/minimal-c
adb shell chmod u+x /data/local/tmp/minimal-c/dlsdk-minimal-c-sample

adb shell 'cd /data/local/tmp/minimal-c && LD_LIBRARY_PATH=/data/local/tmp/minimal-c ./dlsdk-minimal-c-sample'
```

Expected result: the monitor shows solid brand blue (`#0081c6`) for about 10
seconds.


## Troubleshooting

### Could not find dlsdk package config

Ensure:

- `DLSDK_PATH` is absolute and points to the directory that contains `dlsdkConfig.cmake`
- `-DANDROID_PLATFORM` and `-DANDROID_ABI` match existing library directories
- `-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH` is passed at configure time

### `libusb-1.0.so` not found at runtime

Ensure both `libdlsdk.so` and `libusb-1.0.so` were pushed to
`/data/local/tmp/minimal-c` and `LD_LIBRARY_PATH` points there.

### `dlsdk_get_devices` assertion failure

Common causes:

- Not running as root
- USB permission denied

If needed, check adapter visibility and device permissions:

```bash
adb shell
lsusb | grep 17e9
# Example: Bus 002 Device 006: ID 17e9:6015

chmod 666 /dev/bus/usb/002/*
```
