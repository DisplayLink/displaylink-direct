# Running on Android

## Supported target and bundle assumptions

DisplayLink Direct supports the following Android platforms:

- **ABI**: `arm64-v8a`
- **API level**: `android-21`

## Prerequisites

- Android SDK platform tools (`adb`)
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

## Modes of operation

DisplayLink Direct on Android can be integrated using two distinct modes:

- **[Non-rooted mode](#non-rooted-mode-google-play-store-apps)**: Suitable for production applications that target the **Google Play Store** or run on standard, non-rooted consumer devices. The application uses Android's `android.hardware.usb.UsbManager` to request USB host permissions from the user and hands the open USB device file descriptor to DisplayLink Direct. Root access is not required.
- **[Rooted mode](#rooted-mode-developers-and-experiments)**: Suitable for **developers and experiments**. It allows compiling and running standalone native C/C++ command-line executables directly on the device via an `adb` root shell (`adb root`) without building an Android application package (APK). In this mode, `libusb` accesses `/dev/bus/usb/` device nodes directly.

!!! note
    The difference between rooted and non-rooted Android USB integration modes is described in the libusb Android README:
    [https://github.com/libusb/libusb/tree/master/android#readme](https://github.com/libusb/libusb/tree/master/android#readme)

## Non-rooted mode (Google Play Store apps)

Non-rooted mode is the standard integration path for Android applications distributed through the Google Play Store or deployed on standard, non-rooted consumer devices.

In this mode, `libusb` integrates with an Android USB device handle provided by Java
`android.hardware.usb.UsbManager`, so no root access is required:

- Initialise with `dlsdk_initialise_with_config()` and the
  `DLSDK_CONFIG_FLAG_EXTERNAL_USB_DEVICES` flag, so DisplayLink Direct does not
  enumerate the USB bus itself.
- Ask the user for USB permission (or use an intent filter to grant permission automatically),
  open the device with `UsbManager.openDevice()`, and pass its file descriptor to
  `dlsdk_usb_attach_device()`.
- Hand the device back with `dlsdk_usb_detach_device()` before closing the
  `UsbDeviceConnection`.

The [Android Video Wall sample](../code-samples/android-video-wall.md) is a
complete, non-rooted application built this way.

### Handing a USB device to DisplayLink Direct

`UsbDevice.getDeviceName()` has the form `/dev/bus/usb/<bus>/<address>`, which
supplies the bus number and device address:

```kotlin
val usbManager = context.getSystemService(UsbManager::class.java)
val device = usbManager.deviceList.values.first { it.vendorId == 0x17E9 }

// Requires the user to have granted permission for this device.
val connection = usbManager.openDevice(device)
val parts = device.deviceName.split('/')
val busNumber = parts[parts.size - 2].toInt()
val deviceAddress = parts[parts.size - 1].toInt()

// Across JNI: dlsdk_usb_attach_device() / dl::sdk::System::attachUsbDevice()
nativeAttachUsbDevice(connection.fileDescriptor, busNumber, deviceAddress)
```

In native C/C++, initialize DisplayLink Direct for external USB devices and attach the device:

```c
// Initialize DisplayLink Direct to work with external USB devices (not enumerate the bus itself)
dlsdk_config config;
DLSDK_CONFIG_INIT(&config);
config.flags = DLSDK_CONFIG_FLAG_EXTERNAL_USB_DEVICES;
dlsdk_initialise_with_config(&config);

// Hand the USB device (obtained from Android UsbManager) to DisplayLink Direct
dlsdk_usb_device_params params;
DLSDK_USB_DEVICE_PARAMS_INIT(&params, fileDescriptor, busNumber, deviceAddress);
dlsdk_usb_attach_device(&params);
```

!!! warning
    The file descriptor stays owned by the application. It must remain open and
    unused for USB transfers until `dlsdk_usb_detach_device()` returns, or until
    `dlsdk_teardown()` completes. In practice this means the
    `UsbDeviceConnection` must not be closed while the device is attached.

!!! note
    `dlsdk_usb_attach_device()` only hands the device over. Enumeration of the
    device and its displays completes asynchronously, so wait for
    `DLSDK_HOTPLUG_EVENT_DEVICE_ARRIVED` through
    `dlsdk_register_hotplug_callback()`, or poll `dlsdk_get_devices()`, before
    using the device.

### Application manifest

```xml
<uses-feature android:name="android.hardware.usb.host" android:required="true" />
```

Declaring an `android.hardware.usb.action.USB_DEVICE_ATTACHED` intent filter
that matches the DisplayLink USB vendor ID (`0x17E9`, decimal `6121`) lets
Android grant USB permission implicitly when the adapter is plugged in:

```xml
<!-- res/xml/usb_device_filter.xml -->
<resources>
    <usb-device vendor-id="6121" />
</resources>
```

## Rooted mode (Developers and experiments)

Rooted mode is intended for developers, rapid prototyping, and command-line experiments. It allows you to build and run native C/C++ executables directly from an `adb` root shell without packaging an Android application.

This mode uses native USB access directly from `libusb` on device nodes, requiring a root shell (`adb root`) or read/write access to the USB device nodes (`/dev/bus/usb/`).

### Building with CMake

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

### Deploying and running

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
`/data/local/tmp/minimal-c` (or bundled into your application APK) and `LD_LIBRARY_PATH` points there.

### USB device access and permission errors

Common causes in rooted mode:

- Not running as root
- USB permission denied

If needed, check adapter visibility and device permissions:

```bash
adb shell
lsusb | grep 17e9
# Example: Bus 002 Device 006: ID 17e9:6015

chmod 666 /dev/bus/usb/002/*
```

In non-rooted mode, ensure the user approved the Android USB permission prompt or that the USB device filter XML matches vendor ID `6121` (`0x17E9`).
