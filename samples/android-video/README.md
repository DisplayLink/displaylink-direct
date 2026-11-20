# DisplayLink Android Video Wall Sample (`android-video`)

A **non-rooted** Android application that spreads a single MP4 video or still
image across the monitors attached to a DisplayLink adapter.

It is the Android counterpart of [`samples/video/video-wall.cpp`](../video/video-wall.cpp):
the wall layout and frame slicing are the same, but the media pipeline is built
from Android components instead of VLC or GStreamer.

Full documentation: <https://displaylink.github.io/displaylink-direct/code-samples/android-video-wall/>

## Pipeline

```text
ExoPlayer (MediaCodec) → SurfaceTexture → OpenGL ES → AHardwareBuffer → DisplayLink Direct
```

- **ExoPlayer** decodes the MP4 with the platform hardware decoder into a
  `SurfaceTexture`.
- A **GL compositor** scales that external texture into a composite frame buffer
  the size of the whole wall, preserving the video aspect ratio.
- The composite is an **`AHardwareBuffer`** bound through an `EGLImage`, so the
  pixels the DisplayLink encoder reads are the pixels the GPU wrote — there is
  no `glReadPixels` copy in the pipeline.
- A **present thread** maps the finished buffer and calls `dlsdk_display_show()`
  once per display with a different offset into the same buffer, then
  `dlsdk_display_wait_on_show()` on each.

Three composite buffers are kept in flight, so encoding one frame overlaps with
rendering the next. If the encoder falls behind, the oldest queued frame is
dropped instead of delaying playback.

### Wall layout

| Displays | Arrangement |
|----------|-------------|
| 4 | 2 × 2 grid |
| Any other count | Single column, stacked vertically |

All monitors must report the same resolution.

### Non-rooted USB access

DisplayLink Direct is initialised with `DLSDK_CONFIG_FLAG_EXTERNAL_USB_DEVICES`,
so it never scans the USB bus. The app finds adapters through
`android.hardware.usb.UsbManager`, asks the user for permission, and hands the
open file descriptor to `dlsdk_usb_attach_device()`. On unplug it calls
`dlsdk_usb_detach_device()` before closing the `UsbDeviceConnection`.

## Source layout

| Path | Purpose |
|------|---------|
| `app/src/main/cpp/dlsdk_session.*` | DisplayLink Direct system, USB handover, wall layout, frame submission |
| `app/src/main/cpp/gl_compositor.*` | EGL context, external texture, `AHardwareBuffer` render targets |
| `app/src/main/cpp/video_engine.*` | Render and present threads, composite buffer pool |
| `app/src/main/cpp/jni_bridge.cpp` | JNI entry points |
| `app/src/main/kotlin/.../NativeVideo.kt` | Kotlin facade over the native library |
| `app/src/main/kotlin/.../DisplayLinkUsbController.kt` | `UsbManager` permission and attach/detach handling |
| `app/src/main/kotlin/.../VideoWallSession.kt` | ExoPlayer, `SurfaceTexture` and session lifecycle |

## Requirements

- **Minimum Android version**: Android 15 (API level 35)
- **ABI**: `arm64-v8a`
- **Build system**: Gradle 9.8.2 / AGP 9.5.0, JDK 21
- A DisplayLink adapter with monitors attached, connected through a powered hub
  or a port that supplies enough power.

### Native dependencies

Both libraries are read from the DisplayLink Direct bundle and packaged into the
APK:

| Library | Location |
|---------|----------|
| `libdlsdk.so` | `libs/android-21/arm64-v8a/` |
| `libusb-1.0.so` | `libs/android-21/arm64-v8a/` |

`libdlsdk.so` links against `libusb-1.0.so`, so the build fails with an
actionable message if either is missing. If your bundle does not ship libusb,
build it (>= 1.0.27) for Android and drop it next to `libdlsdk.so`, or into
`app/src/main/jniLibs/arm64-v8a/`:

```bash
export NDK=$ANDROID_HOME/ndk/28.2.13676358
git clone https://github.com/libusb/libusb.git
cd libusb/android/jni
"${NDK}"/ndk-build USE_PC_NAME=1 APP_ALLOW_MISSING_DEPS=true \
    APP_MODULES="usb-1.0" APP_CFLAGS="-DANDROID_OS"

cp ../libs/arm64-v8a/libusb-1.0.so <repo>/libs/android-21/arm64-v8a/
```

## Building

```bash
cd samples/android-video

# Build the debug APK (downloads the NDK and CMake on first run)
./gradlew assembleDebug

# Run unit tests
./gradlew test

# Install straight onto a connected device
./gradlew installDebug
```

The APK is written to `app/build/outputs/apk/debug/app-debug.apk`.

By default the build reads the DisplayLink Direct bundle from the root of this
repository. Point it elsewhere with `-Pdlsdk.root`:

```bash
./gradlew assembleDebug -Pdlsdk.root=/path/to/dlsdk
```

If `java` is not on your `PATH`, use the JDK bundled with Android Studio:

```bash
JAVA_HOME=/path/to/android-studio/jbr ./gradlew assembleDebug
```

### Using Android Studio

1. Select **Open** and choose the `samples/android-video` directory.
2. Select your device from the run target dropdown.
3. Click **Run** (`Shift + F10`).

## Running

```bash
# Install and launch
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.displaylink.video/.MainActivity
```

1. Connect the DisplayLink adapter and approve the USB permission dialog.
2. Tap **Choose Video / Image** and pick an MP4 or an image.
3. Leave the mode on **Wall**, pick a session timeout and tap **Start Session**.

Watch the pipeline, which logs its frame rate every five seconds:

```bash
adb logcat -c && adb logcat -s dlvideo:V VideoWallSession:V DisplayLinkUsb:V
```

To uninstall:

```bash
adb uninstall com.displaylink.video
```

## Features

- Media picker for video (`.mp4`, `.mkv`, `.webm`, …) and image files, with
  metadata extraction (name, MIME type, size, resolution, duration).
- Looping playback and a session timeout of 1, 10 or 60 minutes or 10 hours,
  with a countdown and progress indicator.
- The session keeps the screen on and stops automatically when the timeout
  expires, when playback ends without looping, or when the adapter is unplugged.

## Limitations

- Only **Wall** mode is implemented. Clone is disabled in the UI.
- Audio is muted, since the sample drives displays only.
- All monitors must run at the same resolution.
- Automatic firmware updates are disabled; the adapter firmware must already be
  compatible with the DisplayLink Direct version in use.
