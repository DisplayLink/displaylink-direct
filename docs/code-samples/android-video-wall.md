# Android Video Wall

## Overview

The Android Video Wall sample is a complete, **non-rooted** Android application
that spreads a single MP4 video or still image across the monitors attached to a
DisplayLink adapter.

It is the Android counterpart of the [Video Wall](video-wall.md) sample: the
wall layout and frame slicing match
[samples/video/video-wall.cpp]({{ config.repo_url }}/blob/{{ main_branch }}/samples/video/video-wall.cpp),
but the media pipeline is built from Android components instead of VLC or
GStreamer.

## Source Code

The complete implementation is available in
[samples/android-video]({{ config.repo_url }}/tree/{{ main_branch }}/samples/android-video).

## How it works

```text
ExoPlayer (MediaCodec) → SurfaceTexture → OpenGL ES → AHardwareBuffer → DisplayLink Direct
```

1. **ExoPlayer** decodes the MP4 with the platform hardware decoder and renders
   each frame into a `SurfaceTexture`.
2. A **GL compositor** on its own thread scales that external texture into a
   composite frame buffer the size of the whole wall, preserving the video
   aspect ratio.
3. The composite is an **`AHardwareBuffer`** bound through an `EGLImage`, so the
   pixels the DisplayLink encoder reads are exactly the pixels the GPU wrote.
   There is no `glReadPixels` copy in the pipeline.
4. A **present thread** maps the finished buffer and calls
   `dlsdk_display_show()` once per display with a different offset into the same
   buffer, then `dlsdk_display_wait_on_show()` on each of them.

Three composite buffers are kept in flight, so encoding one frame overlaps with
rendering the next. If the encoder falls behind, the oldest queued frame is
dropped rather than delaying playback.

### Wall layout

The layout matches the C++ Video Wall sample:

| Displays | Arrangement |
|----------|-------------|
| 4        | 2 × 2 grid |
| Any other count | Single column, stacked vertically |

All monitors must report the same resolution.

### USB handover

DisplayLink Direct is initialised with `DLSDK_CONFIG_FLAG_EXTERNAL_USB_DEVICES`,
so it never scans the USB bus. The application finds adapters with
`android.hardware.usb.UsbManager`, asks the user for permission, and hands the
open file descriptor to `dlsdk_usb_attach_device()`.

See [Non-rooted mode](../getting-started/installation-android.md#non-rooted-mode-google-play-store-apps)
for detailed information on this integration flow, USB intent filters, and
permission management. In this sample, the Kotlin side is implemented in
[DisplayLinkUsbController.kt]({{ config.repo_url }}/blob/{{ main_branch }}/samples/android-video/app/src/main/kotlin/com/displaylink/video/DisplayLinkUsbController.kt) and native handling is in [dlsdk_session.cpp]({{ config.repo_url }}/blob/{{ main_branch }}/samples/android-video/app/src/main/cpp/dlsdk_session.cpp).

#### Kotlin

```kotlin
val usbManager = context.getSystemService(UsbManager::class.java)
val device = usbManager.deviceList.values.first { it.vendorId == 0x17E9 }

// Requires the user to have granted permission for this device
val connection = usbManager.openDevice(device) ?: return
val parts = device.deviceName.split('/').filter { it.isNotEmpty() }
val busNumber = parts[parts.size - 2].toInt()
val deviceAddress = parts[parts.size - 1].toInt()

// Pass the file descriptor across JNI to native code
nativeAttachUsbDevice(connection.fileDescriptor, busNumber, deviceAddress)
```

#### C / C++

```C++
// Initialise with external USB devices flag so DisplayLink Direct does not scan the bus
dlsdk_config config;
DLSDK_CONFIG_INIT(&config);
config.flags = DLSDK_CONFIG_FLAG_EXTERNAL_USB_DEVICES;
dlsdk_initialise_with_config(&config);

// Hand over the opened USB device
dlsdk_usb_device_params params;
DLSDK_USB_DEVICE_PARAMS_INIT(&params, fileDescriptor, busNumber, deviceAddress);
dlsdk_usb_attach_device(&params);

// On unplug or teardown, detach before closing UsbDeviceConnection
dlsdk_usb_detach_device(busNumber, deviceAddress);
```    

## Building and Running

### Prerequisites

- Android Studio, or the Android SDK with a JDK 21. The NDK and CMake are
  downloaded automatically on the first build.
- An `arm64-v8a` Android 16 (API level 36) device or newer with USB host support.
- A DisplayLink adapter with monitors attached, connected through a powered hub
  or a USB-C port that supplies enough power.
- `libdlsdk.so` and `libusb-1.0.so` for `arm64-v8a` in
  `libs/android-21/arm64-v8a/`.

`libdlsdk.so` links against `libusb-1.0.so`, so both libraries are packaged into
the APK and the build fails with an actionable message if either is missing. If
your bundle does not ship libusb, build it (>= 1.0.27) from the upstream
repository:

```bash
export NDK=$ANDROID_HOME/ndk/28.2.13676358
git clone https://github.com/libusb/libusb.git
cd libusb/android/jni
"${NDK}"/ndk-build USE_PC_NAME=1 APP_ALLOW_MISSING_DEPS=true \
    APP_MODULES="usb-1.0" APP_CFLAGS="-DANDROID_OS"

cp ../libs/arm64-v8a/libusb-1.0.so <repo>/libs/android-21/arm64-v8a/
```

### Build Instructions

```bash
cd samples/android-video

# Build the debug APK
./gradlew assembleDebug

# Run unit tests
./gradlew test
```

By default the build reads the DisplayLink Direct bundle from the root of this
repository. Point it elsewhere with `-Pdlsdk.root`:

```bash
./gradlew assembleDebug -Pdlsdk.root=/path/to/dlsdk
```

If `java` is not on your `PATH`, use the JDK bundled with Android Studio:

```bash
JAVA_HOME=/path/to/android-studio/jbr ./gradlew assembleDebug
```

### Installing and running

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.displaylink.video/.MainActivity
```

1. Connect the DisplayLink adapter and approve the USB permission dialog.
2. Tap **Choose Video / Image** and pick an MP4 or an image.
3. Leave the mode on **Wall**, pick a session timeout and tap **Start Session**.

The pipeline logs its frame rate every five seconds:

```bash
adb logcat -c && adb logcat -s dlvideo:V VideoWallSession:V DisplayLinkUsb:V
```

### Notes

- Only **Wall** mode is implemented. Clone is disabled in the UI.
- Audio is muted, since the sample drives displays only.
- The session keeps the screen on and stops automatically when the timeout
  expires, when playback ends without looping, or when the adapter is unplugged.
- The composite frame buffer carries one spare row, because
  `dlsdk_display_show()` requires `pixelsSize` to be at least
  `stride * displayHeight` and the bottom-right region of a 2 × 2 wall reaches
  one display row past the composite area.
