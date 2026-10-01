# API Documentation

!!! info

    See [Minimal Example](minimal-example.md) for a minimal usage example that powers on a display and presents a solid-color frame.

This page documents the C API used to display pixel buffers across multiple
displays.

## Contact and Support

For support, contact your DisplayLink representative or email
technical-enquiries@synaptics.com.

## Thread Safety

The API is thread-safe: it may be called concurrently from multiple threads, including on the same handle (e.g. `dlsdk_device_handle`, `dlsdk_display_handle`, `dlsdk_dpaux_handle`), without external locking. Concurrent calls on the same handle are serialised internally, so a long-running call such as `dlsdk_device_update_firmware()` will block other threads until it completes.

The following remain the caller's responsibility:

- `dlsdk_initialise()`, `dlsdk_initialise_with_config()` and `dlsdk_teardown()` are not thread-safe. Call them from a single thread, and ensure no other API call is in progress on any thread when calling `dlsdk_teardown()`.
- All handles are owned by the DisplayLink Direct and are invalidated on `dlsdk_teardown()`; they must not be used after that point.
- Serialization of concurrent calls on the same handle is handled internally, but the order is time-dependent. Callers must still sequence dependent operations.
- A hotplug callback must not call `dlsdk_register_hotplug_callback()` or `dlsdk_unregister_hotplug_callback()`, as this will deadlock.

## C API

The C API is declared in `include/dlsdk/dlsdk.h` and is the primary,
ABI-stable interface exposed by DisplayLink Direct. The C++ helper classes
(`dl::sdk::System`, `dl::sdk::DeviceHandle`, `dl::sdk::DisplayHandle`, ...) are
thin wrappers around these same functions.

All functions are exported via `DLSDK_EXPORT` and are safe to call from C or
C++ (the header wraps declarations in `extern "C"`).

Objects are created in the following order: a `System` is initialised first,
which is then used to enumerate `Device`s, which in turn expose their
attached `Display`s. This section documents the API in that same order:
System, Device, Display, then DP AUX.

### Types

Custom types used across the API.

| Type | Description |
| ---- | ----------- |
| `dlsdk_config_flags` | Configuration flags used to initialise the DisplayLink Direct. |
| `dlsdk_config` | Configuration structure used to initialise the DisplayLink Direct with custom options. |
| `dlsdk_rect` | An abstract representation of a rectangular area, used for display resolution and size information. |
| `dlsdk_dirty_rect` | A rectangular area of a pixel buffer, relative to its top-left corner |
| `dlsdk_display_mode` | A display mode represents a specific resolution and refresh rate at which a display can operate. |
| `dlsdk_display_timing` | Detailed video timing parameters used to power on a display with a fully custom timing, rather than one of the modes reported by `dlsdk_display_modes()` |
| `dlsdk_display_signal_mode` | Display signal mode used by the display signal. |
| `dlsdk_display_power_on_options` | Options used when powering on a display. |
| `dlsdk_status` | API method status return codes. |
| `dlsdk_pixel_format` | API-supported pixel formats. |
| `dlsdk_hdr_metadata_flags` | Flags identifying the static HDR metadata format associated with a frame. |
| `dlsdk_hdr_smpte2086` | SMPTE ST 2086 mastering display color volume metadata. |
| `dlsdk_hdr_cta861_3` | CTA-861.3 content light level metadata. |
| `dlsdk_hdr10_metadata` | HDR10 static metadata payload. |
| `dlsdk_hdr_static_metadata` | Static HDR metadata associated with a frame. |
| `dlsdk_display_capabilities` | Display capabilities relevant to signal modes and pixel formats. |
| `dlsdk_frame` | Frame description used by `dlsdk_display_show()`. |
| `dlsdk_aux_status` | DP AUX channel return codes. |
| `dlsdk_hotplug_callback_handle` | Opaque handle to a hotplug callback registration, used for unregistering callbacks. |
| `dlsdk_display_handle` | Opaque handle to a Display object. |
| `dlsdk_device_handle` | Opaque handle to a Device object. |
| `dlsdk_dpaux_handle` | Opaque handle to a DP AUX channel object. |
| `dlsdk_hotplug_event` | Hotplug event types. |
| `dlsdk_hotplug_event_data` | Data provided to hotplug callbacks when a hotplug event occurs. |
| `dlsdk_hotplug_callback_fn` | Hotplug callback function type. |

#### `dlsdk_config_flags`

Configuration flags used to initialise the DisplayLink Direct.

| Value | Description |
| ----- | ----------- |
| `DLSDK_CONFIG_FLAG_EMBEDDED_MODE` | In embedded mode, DisplayLink Direct will not use dynamic memory at runtime. |

#### `dlsdk_config`

Configuration structure used to initialise the DisplayLink Direct with custom options.

| Field | Description |
| ----- | ----------- |
| `size` | Size of the structure; is set by `DLSDK_CONFIG_INIT(&config)`. |
| `flags` | Combination of `dlsdk_config_flags` values. |
| `firmwarePath` | Firmware directory path used to update the firmware automatically if required. Set to NULL or an empty string to disable automatic firmware updates. If automatic firmware updates are disabled, the user should ensure that the firmware on the device is compatible with the DisplayLink Direct version; otherwise, the device may not function properly or at all. |

`DLSDK_CONFIG_INIT()` is the default initializer for `dlsdk_config`.

**Details:** Zero-initialises the complete structure, and initialises the size field. After using this macro, callers may set `firmwarePath` and `flags` as needed.

#### `dlsdk_rect`

An abstract representation of a rectangular area, used for display resolution and size information.

| Field | Description |
| ----- | ----------- |
| `width` | Width of the rectangle. |
| `height` | Height of the rectangle. |

#### `dlsdk_dirty_rect`

A rectangular area of a pixel buffer, relative to its top-left corner.

| Field | Description |
| ----- | ----------- |
| `x` | Horizontal offset of the rectangle from the left edge of the pixel buffer, in pixels. |
| `y` | Vertical offset of the rectangle from the top edge of the pixel buffer, in pixels. |
| `width` | Width of the rectangle, in pixels. |
| `height` | Height of the rectangle, in pixels. |

`DLSDK_MAX_DIRTY_RECTS` (128) is the maximum number of dirty rectangles accepted by `dlsdk_display_show()`.

#### `dlsdk_display_mode`

A display mode represents a specific resolution and refresh rate at which a display can operate.

| Field | Description |
| ----- | ----------- |
| `resolution` | Display resolution. |
| `refreshRateHz` | Refresh rate in Hertz. |

#### `dlsdk_display_timing`

Detailed video timing parameters used to power on a display with a fully custom timing, rather than one of the modes reported by `dlsdk_display_modes()`.

| Field | Description |
| ----- | ----------- |
| `horizontalResolution` | Active horizontal resolution, in pixels |
| `verticalResolution` | Active vertical resolution, in pixels |
| `refreshRateMilliHz` | Refresh rate, in milli-Hertz (e.g. 60000 for 60Hz) |
| `hTotal` | Total horizontal line time, in pixels |
| `vTotal` | Total vertical frame time, in lines |
| `hStart` | Offset from the start of the line to the start of active video, in pixels (hsync width + back porch) |
| `vStart` | Offset from the start of the frame to the start of active video, in lines (vsync width + back porch) |
| `hSyncWidth` | Horizontal sync pulse width, in pixels |
| `vSyncWidth` | Vertical sync pulse width, in lines |
| `hSyncPolarityPositive` | Non-zero if the horizontal sync pulse is active-high |
| `vSyncPolarityPositive` | Non-zero if the vertical sync pulse is active-high |

#### `dlsdk_display_signal_mode`

Display signal mode used by the display signal.

| Value | Description |
| ----- | ----------- |
| `DLSDK_DISPLAY_SIGNAL_MODE_SDR` (0) | Standard dynamic range signal. |
| `DLSDK_DISPLAY_SIGNAL_MODE_HDR10` (1) | HDR10 signal. |

#### `dlsdk_display_power_on_options`

Options used when powering on a display.

**Details:** Initialise this structure with `DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)`. Optionally set the `signalMode`, `mode`, and `timing` fields to request a specific display configuration. The `mode` and `timing` pointers are mutually exclusive; when both are NULL, the preferred display mode is used. When using a `signalMode` of `DLSDK_DISPLAY_SIGNAL_MODE_HDR10`, you must use `DLSDK_PIXEL_FORMAT_RGB10` or `DLSDK_PIXEL_FORMAT_XBGR16F` in `dlsdk_display_show()`.

| Field | Description |
| ----- | ----------- |
| `size` | Size of this structure; initialise with `DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)`. |
| `signalMode` | Display signal mode used by the display. |
| `mode` | Optional requested display mode. |
| `timing` | Optional requested custom timing. |

`DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)` zero-initialises the complete structure and initialises a preferred-mode SDR power-on request.

#### `dlsdk_hdr_metadata_flags`

Flags identifying the static HDR metadata format associated with a frame.

| Value | Description |
| ----- | ----------- |
| `DLSDK_HDR_METADATA_FLAG_NONE` (0) | No static HDR metadata. |
| `DLSDK_HDR_METADATA_HDR10` (1) | HDR10 static metadata. |

#### `dlsdk_hdr_smpte2086`

SMPTE ST 2086 mastering display color volume metadata.

**Details:** Chromaticity values are normalised CIE 1931 x/y coordinates in the range [0, 1]. Luminance values are in cd/m^2 (nits).

| Field | Description |
| ----- | ----------- |
| `redX` | Red primary x coordinate. |
| `redY` | Red primary y coordinate. |
| `greenX` | Green primary x coordinate. |
| `greenY` | Green primary y coordinate. |
| `blueX` | Blue primary x coordinate. |
| `blueY` | Blue primary y coordinate. |
| `whiteX` | White point x coordinate. |
| `whiteY` | White point y coordinate. |
| `maxLuminance` | Maximum mastering luminance in cd/m^2 (nits). |
| `minLuminance` | Minimum mastering luminance in cd/m^2 (nits). |

#### `dlsdk_hdr_cta861_3`

CTA-861.3 content light level metadata.

**Details:** Light-level values are in cd/m^2 (nits).

| Field | Description |
| ----- | ----------- |
| `maxContentLightLevel` | Maximum content light level. |
| `maxFrameAverageLightLevel` | Maximum frame-average light level. |

#### `dlsdk_hdr10_metadata`

HDR10 static metadata payload.

| Field | Description |
| ----- | ----------- |
| `smpte2086` | SMPTE 2086 mastering display color volume metadata. |
| `cta861_3` | CTA-861.3 content light level metadata. |

#### `dlsdk_hdr_static_metadata`

Static HDR metadata associated with a frame.

| Field | Description |
| ----- | ----------- |
| `size` | Size of this structure; initialise with `DLSDK_HDR_STATIC_METADATA_INIT(&metadata)`. |
| `flags` | Combination of `dlsdk_hdr_metadata_flags` values. |
| `hdr10` | Complete HDR10 payload when its flag is set. |

`DLSDK_HDR_STATIC_METADATA_INIT(&metadata)` initialises the structure and its nested HDR10 metadata fields.

#### `dlsdk_display_capabilities`

Display capabilities relevant to signal modes and pixel formats.

**Details:** The `signalModeMask` uses the value of `dlsdk_display_signal_mode` as its bit index. The `pixelFormatMask` uses the value of `dlsdk_pixel_format` as its bit index. Use `DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities)` to initialise this structure before calling `dlsdk_display_get_capabilities()`.

| Field | Description |
| ----- | ----------- |
| `size` | Size of this structure; initialise with `DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities)`. |
| `pixelFormatMask` | Supported `dlsdk_pixel_format` values. |
| `signalModeMask` | Supported `dlsdk_display_signal_mode` values. |

`DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities)` zero-initialises the complete structure and initialises the structure size. The display capability query fills the output fields.

#### `dlsdk_status` status codes

API method status return codes.

| Value | Meaning |
| ----- | ------- |
| `DLSDK_SUCCESS` | The call completed successfully. |
| `DLSDK_NOT_ENOUGH_SPACE` | The provided output buffer is too small; reallocate using the returned required size and call again. |
| `DLSDK_UNSUCCESSFUL` | Generic failure. |
| `DLSDK_UNSUCCESSFUL_NO_DEVICE` | No device was found (e.g. after a firmware update, within the expected wait window). |
| `DLSDK_UNSUCCESSFUL_NO_MONITOR` | The display is not ready / no monitor detected. |
| `DLSDK_UNSUCCESSFUL_MONITOR_OFF` | The monitor is powered off. |
| `DLSDK_INVALID_HANDLE` | The provided handle is invalid or unknown. |
| `DLSDK_INVALID_ARGS` | One or more arguments are invalid. |
| `DLSDK_TIMEOUT` | The operation timed out. |
| `DLSDK_NOT_IMPLEMENTED` | The requested functionality is not implemented. |
| `DLSDK_BUSY` | The device aborted a previously submitted frame due to resource constraints; retry `dlsdk_display_show()`. |

#### `dlsdk_pixel_format` pixel formats

API-supported pixel formats.

| Value | Bytes per pixel |
| ----- | --------------- |
| `DLSDK_PIXEL_FORMAT_XRGB` | 4 |
| `DLSDK_PIXEL_FORMAT_XBGR` | 4 |
| `DLSDK_PIXEL_FORMAT_RGB565` | 2 |
| `DLSDK_PIXEL_FORMAT_RGB` | 3 |
| `DLSDK_PIXEL_FORMAT_RGB10` | 4 |
| `DLSDK_PIXEL_FORMAT_XBGR16F` | 8 |

XRGB and XBGR are recommended for maximum compatibility and performance.

#### `dlsdk_frame`

Frame description used by `dlsdk_display_show()`.

| Field | Description |
| ----- | ----------- |
| `size` | Size of this structure, is set by `DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE)`. |
| `format` | The pixel format contained in the pixel buffer. |
| `pixels` | The buffer containing the pixels to be displayed. For optimal performance, a 64-byte aligned pixel buffer address is preferred. |
| `pixelsSize` | Size in bytes of the pixel buffer. Must be non-zero and large enough for display height and effective stride. |
| `stride` | Number of bytes between two consecutive rows. If 0, it is assumed to be bytesPerPixel * displayWidth. For optimal performance, a 64-byte aligned stride is preferred. |
| `dirtyRects` | Array of changed regions. NULL or zero count means full-display update. |
| `dirtyRectCount` | Number of rectangles in `dirtyRects`. Zero when `dirtyRects` is NULL, less than or equal to `DLSDK_MAX_DIRTY_RECTS` otherwise. |
| `staticMetadata` | Optional static HDR metadata. |

`DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE)` is the default initializer for `dlsdk_frame`.

**Details:** Zero-initialises the complete structure, and initialises the required fields. After using this macro, callers may set the optional parameters: `stride`, `dirtyRects`, `dirtyRectCount`, and `staticMetadata` as needed.

#### `dlsdk_aux_status`

DP AUX channel return codes.

| Value | Description |
| ----- | ----------- |
| `DLSDK_AUX_ACK` (0x0) | Standard DP response code |
| `DLSDK_AUX_NATIVE_NAK` (0x1) | Standard DP response code |
| `DLSDK_AUX_NATIVE_DEFER` (0x2) | Standard DP response code |
| `DLSDK_AUX_TIMEOUT` (0x10) | Timeout, no response from downstream |
| `DLSDK_AUX_ERROR` (0x11) | Error detected by DP hardware |
| `DLSDK_AUX_DETACHED` (0x12) | Sink connection no longer present |

#### `dlsdk_hotplug_event`

Hotplug event types: `DLSDK_HOTPLUG_EVENT_DEVICE_ARRIVED`, `DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED`, `DLSDK_HOTPLUG_EVENT_DISPLAY_ARRIVED`, `DLSDK_HOTPLUG_EVENT_DISPLAY_REMOVED`.

#### `dlsdk_hotplug_event_data`

Data provided to hotplug callbacks when a hotplug event occurs.

Handle validity rules:

- On `DLSDK_HOTPLUG_EVENT_DEVICE_ARRIVED`, `device` is valid during the callback and will remain valid until a corresponding `DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED` event is received for the same device.
- On `DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED`, `device` is informational only and must not be used with device API calls.
- On `DLSDK_HOTPLUG_EVENT_DISPLAY_ARRIVED`, `display` is valid during the callback. The value is NULL if the event is not for a display (for example, a device hotplug event).
- On `DLSDK_HOTPLUG_EVENT_DISPLAY_REMOVED`, `display` is informational only and must not be used with display API calls.

| Field | Description |
| ----- | ----------- |
| `version` | Struct version set by the DisplayLink Direct. |
| `event` | The type of hotplug event that occurred. |
| `device` | Handle to the device on which the event occurred. |
| `device_id` | Unique identifier for the device involved in the event. Valid for the duration of the callback. |
| `display` | Handle to the display involved in the event if applicable. |
| `display_id` | Unique identifier for the display involved in the event. Valid for the duration of the callback. |

#### `dlsdk_hotplug_callback_fn`

```c
typedef void (*dlsdk_hotplug_callback_fn)(dlsdk_hotplug_event_data* data, void* user_data);
```

Hotplug callback function type.

**Details:** You must not register or unregister callbacks from within a callback. Callbacks are invoked on an internal DisplayLink Direct worker thread. Callbacks are serialised and delivered in event order.

**Parameters:**

- `data` — Non-null pointer to a struct containing event data.
- `user_data` — User data pointer provided at registration.

**Example:**

=== "C"

    ```c
    #include <stdio.h>
    #include <dlsdk/dlsdk.h>

    static void on_hotplug(dlsdk_hotplug_event_data* data, void* user_data)
    {
        (void)user_data;
        if (!data) {
            return;
        }

        printf("hotplug event=%d device=%s display=%s\n",
               (int)data->event,
               data->device_id ? data->device_id : "<null>",
               data->display_id ? data->display_id : "<null>");
    }

    void register_and_unregister_hotplug_callback(void)
    {
        dlsdk_hotplug_callback_handle cb_handle = NULL;
        dlsdk_status st = dlsdk_register_hotplug_callback(on_hotplug, NULL, &cb_handle);
        if (st == DLSDK_SUCCESS) {
            /* ... run app logic ... */
            dlsdk_unregister_hotplug_callback(cb_handle);
        }
    }
    ```

=== "C++"

    ```cpp
    #include <iostream>
    #include <dlsdk/dlsdk.h>

    dl::sdk::System system;

    auto callback = [](const dl::sdk::HotplugEventData& event) {
    std::cout << "hotplug event=" << static_cast<int>(event.event)
              << " device=" << event.device_id
              << " display=" << event.display_id
              << std::endl;
    };

    dl::sdk::CallbackHandle callbackHandle;
    auto handle = system.registerHotplugCallback(callback, &callbackHandle);
    // ... run app logic ...
    system.unregisterHotplugCallback(callbackHandle);
    ```

=== "Python"

    ```python
    import dlsdk

    def on_hotplug(event_data):
        print(
            f"hotplug event={int(event_data.event)} "
            f"device={event_data.device_id} display={event_data.display_id}"
        )

    status, callback_handle = dlsdk.register_hotplug_callback(on_hotplug)
    if status == dlsdk.dlsdk_status.DLSDK_SUCCESS:
        # ... run app logic ...
        dlsdk.unregister_hotplug_callback(callback_handle)
    ```

### System API

Functions not related to a particular device or display.

#### `dlsdk_initialise`

```c
dlsdk_status dlsdk_initialise(void);
```

Function used to initialise the internals of the DisplayLink Direct.

**Details:** This method must be called by the user prior to invoking any other functionality. In OO parlance, it can be thought of as a DisplayLink Direct constructor.

**Returns:** DLSDK_SUCCESS on success, otherwise an error code.

#### `dlsdk_initialise_with_config`

```c
dlsdk_status dlsdk_initialise_with_config(const dlsdk_config* config);
```

Function used to initialise the DisplayLink Direct with a specific configuration.

**Details:** This method must be called by the user prior to invoking any other functionality. In OO parlance, it can be thought of as a DisplayLink Direct constructor.

**Parameters:**

- `config` — The configuration structure containing the firmware path and other settings. See `dlsdk_config` for details.

**Returns:** DLSDK_SUCCESS on success, otherwise an error code.

#### `dlsdk_teardown`

```c
void dlsdk_teardown(void);
```

Function used to clean up the DisplayLink Direct internals when a user no longer requires its use.

**Details:** This method must be called by the user prior the application exiting.

#### `dlsdk_version`

```c
const char* dlsdk_version(void);
```

Function used to obtain the version of the SDK.

**Returns:** A string containing the DisplayLink Direct version in Python PEP 440 format.

#### `dlsdk_get_devices`

```c
dlsdk_status dlsdk_get_devices(dlsdk_device_handle* devices, unsigned int* size);
```

Function used to obtain the attached devices.

**Details:** The caller must allocate enough memory to store the device handles
(`dlsdk_device_handle`) before calling this method. If there are more attached
devices than the allocated memory can hold, the function returns
`DLSDK_NOT_ENOUGH_SPACE`. The user must then allocate memory equal to the
returned size before calling the function again.

**Parameters:**

- `devices` — A pointer to a contiguous memory block large enough to store the connected device handles. If NULL is passed, the function returns only the number of connected devices in the `size` parameter.
- `size` — [in]: the number of device handles that can be stored in the devices parameter. [out]: The number of attached devices.

**Returns:** DLSDK_NOT_ENOUGH_SPACE if there are more attached devices than allocated memory for storing `dlsdk_device_handle`.

#### `dlsdk_restart_device`

```c
dlsdk_status dlsdk_restart_device(dlsdk_device_handle device);
```

Function used to restart a device.

**Details:** On calling this function, the device will be restarted. The device and display handles remain valid after restart, but the rendering pipeline must be reestablished and any displays must be powered on again. In embedded mode, the device and displays will no longer be accessible through their handles and the user must reinitialize the system to obtain new handles.

**Parameters:**

- `device` — Device handle. Must be valid and non-NULL.

#### `dlsdk_register_hotplug_callback`

```c
dlsdk_status dlsdk_register_hotplug_callback(dlsdk_hotplug_callback_fn cb, void* user_data, dlsdk_hotplug_callback_handle* outHandle);
```

Registers a hotplug callback function.

**Details:** This function registers a callback to be invoked when a hotplug event occurs and returns a registration handle for later unregistration. Only one callback can be registered, and registering a new callback will overwrite the previous one. The callback will be invoked for all devices and displays currently connected and for all subsequent hotplug events from the moment the callback is registered. All callbacks are invoked on an internal DisplayLink Direct worker thread. When a system is destroyed, all hotplug callbacks will be automatically unregistered and their handles invalidated.

**Parameters:**

- `cb` — the function to be invoked when a hotplug event occurs. Must be non-NULL.
- `user_data` — user data to pass to the callback function. Can be NULL if not needed.
- `outHandle` — Callback registration handle. Must be non-NULL. On success, receives a non-NULL handle.

**Returns:** DLSDK_SUCCESS on success, otherwise an error code.

#### `dlsdk_unregister_hotplug_callback`

```c
dlsdk_status dlsdk_unregister_hotplug_callback(dlsdk_hotplug_callback_handle handle);
```

Unregisters a hotplug callback.

**Details:** You must not register or unregister callbacks from within a callback.
The callback will not be invoked after this function returns successfully. If
the callback is currently running, this function blocks until it completes
before unregistering and returning.

**Parameters:**

- `handle` — The callback registration handle returned during registration.

**Returns:** DLSDK_SUCCESS on success, DLSDK_INVALID_HANDLE if handle is unknown or already unregistered, otherwise an error code.

### Device API

Functions for manipulating/controlling attached devices.

#### `dlsdk_device_id`

```c
const char* dlsdk_device_id(dlsdk_device_handle device);
```

Function used to obtain the unique identifier of the device.

**Details:** Based on the device's USB serial number, so the identifier persists across unplugging and power cycling. Example: "USB_6015-10096418", where 6015 is the USB product ID (in hex) and 10096418 is the serial number. The DisplayLink USB vendor ID (0x17e9) is not included.

**Parameters:**

- `device` — Device handle. Must be valid and non-NULL.

**Returns:** A string containing the unique device identifier.

**Example:**

=== "C"

    ```c
    const char* id = dlsdk_device_id(device);
    printf("%s\n", id); // e.g. USB_6015-10096418
    ```

=== "C++"

    ```cpp
    std::string id = device.id();
    std::cout << id << std::endl; // e.g. USB_6015-10096418
    ```

=== "Python"

    ```python
    device_id = device.id()
    print(device_id)  # e.g. USB_6015-10096418
    ```

#### `dlsdk_device_firmware_version`

```c
const char* dlsdk_device_firmware_version(dlsdk_device_handle device);
```

Function used to obtain the firmware version of the device.

**Parameters:**

- `device` — Device handle. Must be valid and non-NULL.

**Returns:** A string containing the firmware version of the device.

#### `dlsdk_device_get_displays`

```c
dlsdk_status dlsdk_device_get_displays(dlsdk_device_handle device, dlsdk_display_handle* displays, unsigned int* size);
```

Function used to obtain the attached displays for a given device.

**Details:** The caller must allocate enough memory to store the display handles
(`dlsdk_display_handle`) before calling this method. If there are more attached
displays than the allocated memory can hold, the function returns
`DLSDK_NOT_ENOUGH_SPACE`. The user must then allocate memory equal to the
returned size before calling the function again. Display handles are owned by
their device and must not be freed by the caller; they remain valid until the
associated device handle is released with `dlsdk_teardown()`.

**Parameters:**

- `device` — Device handle. Must be valid and non-NULL.
- `displays` — A pointer to a contiguous memory block large enough to store the display handles connected to the device. If NULL, the function returns only the number of attached displays in the `size` parameter.
- `size` — [in]: the number of display handles that can be stored in the displays parameter. [out]: The number of attached displays.

**Returns:** DLSDK_NOT_ENOUGH_SPACE if there are more attached displays than allocated memory for storing `dlsdk_display_handle`.

#### `dlsdk_device_get_dpaux`

```c
dlsdk_dpaux_handle dlsdk_device_get_dpaux(dlsdk_device_handle device, unsigned int output);
```

Function used to obtain a handle to the DP AUX channel available on a given device.

**Details:** Some devices may not have a DP AUX channel, in which case this
function returns NULL. This function is useful when low-level access to the DP
AUX channel is required and the display is not enumerated automatically.
Otherwise, obtain the DP AUX channel handle through the display object by using
`dlsdk_display_get_dpaux()`.

**Parameters:**

- `device` — Device handle. Must be valid and non-NULL.
- `output` — The output index for which to obtain the DP AUX channel handle.

**Returns:** A handle to the DP AUX channel, or NULL if the device does not
support the channel, the output is not a DisplayPort output, or the output does
not exist.

#### `dlsdk_device_update_firmware`

```c
dlsdk_status dlsdk_device_update_firmware(dlsdk_device_handle device, const char* firmwarePath);
```

Function used to update the firmware of a device.

**Details:** This synchronous function updates the firmware on a specific
device and blocks until the process is complete, which may take up to 30
seconds. It is required when the DisplayLink Direct is initialised with a null firmware path
and can also be used to update firmware on demand. Keep the firmware up to date
to ensure optimal device performance, reliability, and access to the latest
features. After a successful update, the device and display handles remain
valid, but the rendering pipeline must be reestablished and the display must be
powered on again.

**Parameters:**

- `device` — The device handle. Must be valid and non-NULL.
- `firmwarePath` — The path to the firmware directory provided by DisplayLink. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS on success, otherwise an error code. DLSDK_UNSUCCESSFUL_NO_DEVICE if the device is not found within 15 seconds after firmware update.

See [Firmware Update](../getting-started/firmware-update.md) for more on
firmware-related workflows.

### Display API

Functions for manipulating/controlling displays attached to a device.

#### `dlsdk_display_id`

```c
const char* dlsdk_display_id(dlsdk_display_handle display);
```

Function used to obtain the unique identifier of the display.

**Details:** Identifies a single output on a device; a device with multiple outputs (e.g. up to 4 HDMI connectors) exposes one identifier per output. Example: "USB_6015-10096418^0", "USB_6015-10096418^1", "USB_6015-10096418^2", "USB_6015-10096418^3", where 6015 is the USB product ID (in hex), 10096418 is the serial number, and the suffix after ^ is the output index. The DisplayLink USB vendor ID (0x17e9) is not included.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:** A string containing the device and head identifiers.

**Example:**

=== "C"

    ```c
    const char* id = dlsdk_display_id(display);
    printf("%s\n", id); // e.g. USB_6015-10096418^0
    ```

=== "C++"

    ```cpp
    std::string id = display.id();
    std::cout << id << std::endl; // e.g. USB_6015-10096418^0
    ```

=== "Python"

    ```python
    display_id = display.id()
    print(display_id)  # e.g. USB_6015-10096418^0
    ```

#### `dlsdk_display_size`

```c
struct dlsdk_rect dlsdk_display_size(dlsdk_display_handle display);
```

Function used to obtain the resolution of the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:** The resolution of the display.

#### `dlsdk_display_edid`

```c
dlsdk_status dlsdk_display_edid(dlsdk_display_handle display, uint8_t* data, uint32_t* len);
```

Function used to obtain the EDID of the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `data` — Buffer to store the EDID data. If null, the function will return the required buffer size in len.
- `len` — Length of the data buffer. On input, should be set to the size of the data buffer. On output, will be set to the actual length of the EDID data written to the buffer, or the required buffer size if buffer is null. If the buffer is too small, the data will be truncated.

**Returns:** DLSDK_SUCCESS if the EDID was obtained successfully, or the reason
for failure otherwise.

#### `dlsdk_display_modes`

```c
dlsdk_status dlsdk_display_modes(dlsdk_display_handle display, dlsdk_display_mode* modes, unsigned int* count);
```

Function used to retrieve a list of supported display modes for the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `modes` — Buffer to store the display modes. If null, the function will return the required buffer size in count.
- `count` — Number of display modes that can be stored in the modes buffer. On input, should be set to the size of the modes buffer. On output, will be set to the actual number of display modes written to the buffer, or the required buffer size if buffer is null.

**Returns:** DLSDK_SUCCESS if the display modes were obtained successfully, or
the reason for failure otherwise.

#### `dlsdk_display_get_capabilities`

```c
dlsdk_status dlsdk_display_get_capabilities(dlsdk_display_handle display, dlsdk_display_capabilities* capabilities);
```

Function used to obtain signal mode and pixel format capabilities for the display.

**Details:** Initialise capabilities with `DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities)` before calling this function; the macro sets `capabilities->size` to `sizeof(dlsdk_display_capabilities)`. The `signalModeMask` uses the value of `dlsdk_display_signal_mode` as its bit index. The `pixelFormatMask` uses the value of `dlsdk_pixel_format` as its bit index.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `capabilities` — Buffer to store the display capabilities. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS if the capabilities were obtained successfully, or the reason for failure otherwise.

#### `dlsdk_display_preferred_mode`

```c
dlsdk_status dlsdk_display_preferred_mode(dlsdk_display_handle display, dlsdk_display_mode* mode);
```

Function used to obtain the preferred display mode for the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `mode` — Buffer to store the preferred display mode.

**Returns:** DLSDK_SUCCESS if the preferred display mode was obtained
successfully, or the reason for failure otherwise.

#### `dlsdk_display_power_on`

```c
dlsdk_status dlsdk_display_power_on(dlsdk_display_handle display);
```

Function used to power on the display in its preferred mode and SDR signal mode.

**Details:** This is equivalent to calling `dlsdk_display_power_on_with_options()` with an options structure initialised by `DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)`.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `dlsdk_display_power_on_with_options`

```c
dlsdk_status dlsdk_display_power_on_with_options(dlsdk_display_handle display, const dlsdk_display_power_on_options* options);
```

Function used to power on the display with custom options.

**Details:** The display is powered on using the preferred display mode when `options->mode` and `options->timing` are NULL.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `options` — Power-on options. Must be valid and non-NULL. Initialise it with `DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)` before setting optional fields. The mode and timing pointers are mutually exclusive.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `dlsdk_display_power_off`

```c
dlsdk_status dlsdk_display_power_off(dlsdk_display_handle display);
```

Function used to power off the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `dlsdk_display_show`

```c
dlsdk_status dlsdk_display_show(dlsdk_display_handle display, const dlsdk_frame* frame);
```

Function to present frame contents on the display.

**Details:** This is a non-blocking call and returns before the pixels are displayed. To receive notification when the pixels have been displayed, call `dlsdk_display_wait_on_show()`. The frame can describe either a full-screen update or a partial update using dirty rectangles.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `frame` — Frame description. Must be non-NULL and initialised with `DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE)`.
    Frame buffer requirements:
    - `frame->pixels` must be non-NULL.
    - `frame->pixelsSize` must be non-zero and at least `frame->stride * displayHeight`.
    - If `frame->stride` is zero, it is assumed to be bytesPerPixel * displayWidth; otherwise it must be greater than or equal to this value.
    - For optimal performance, 64-byte alignment of both `frame->pixels` address and `frame->stride` is preferred.

    Dirty rectangle requirements:
    - `frame->dirtyRects` is an optional hint to the display of which regions of the pixel buffer have changed since the previous frame, allowing implementations that support partial updates to only update those regions.
    - The rectangles must cover everything that changed, as changes outside of them may not be presented on the display.
    - The `frame->pixels` buffer must still describe the entire display, as the DisplayLink Direct may access regions outside of those given.
    - Rectangles are clipped to the display area, and rectangles that fall entirely outside it are ignored.
    - If `frame->dirtyRects` is NULL, a full-screen update is assumed.
    - `frame->dirtyRectCount` must be <= `DLSDK_MAX_DIRTY_RECTS`.
    - If `frame->dirtyRectCount` is non-zero, `frame->dirtyRects` must be non-NULL.

    When the display is powered on in HDR10 mode, the frame format must provide at
    least 10 bits per channel of source precision and be supported by the display
    capabilities. If `frame->staticMetadata` is non-NULL, initialise it with
    `DLSDK_HDR_STATIC_METADATA_INIT(&metadata)`, then its metadata is applied synchronously
    with the frame.

**Returns:** DLSDK_SUCCESS if the frame was successfully submitted for rendering, or an error code on failure. The caller should check the render and frame transmission status with dlsdk_display_wait_on_show(). DLSDK_INVALID_ARGS for invalid frame parameters.

#### `dlsdk_display_wait_on_show`

```c
dlsdk_status dlsdk_display_wait_on_show(dlsdk_display_handle display);
```

Function used to block for notification that pixels have been displayed after calling dlsdk_display_show().

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:**

- DLSDK_SUCCESS when the pixels have been displayed.
- DLSDK_UNSUCCESSFUL_NO_MONITOR when display is not ready.
- DLSDK_BUSY when the device aborted a previously submitted frame due to resource constraints. The caller should retry `dlsdk_display_show()` with the frame to be displayed, which will typically involve more aggressive compression, or proceed with a new frame if the previous frame is no longer relevant.
- DLSDK_TIMEOUT if waiting for the display to be ready timed out.
- DLSDK_UNSUCCESSFUL on unknown error.

#### `dlsdk_display_wait_on_show_for`

```c
dlsdk_status dlsdk_display_wait_on_show_for(dlsdk_display_handle display, uint32_t timeoutMs);
```

Function used to block until the pixels have been displayed after calling
`dlsdk_display_show()`, with a timeout option.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `timeoutMs` — Maximum time to wait in milliseconds.

**Returns:**

- DLSDK_SUCCESS when the pixels have been displayed.
- DLSDK_UNSUCCESSFUL_NO_MONITOR when display is not ready.
- DLSDK_BUSY when the device aborted a previously submitted frame due to resource constraints. The caller should retry `dlsdk_display_show()` with the frame to be displayed, which will typically involve more aggressive compression, or proceed with a new frame if the previous frame is no longer relevant.
- DLSDK_TIMEOUT if waiting for the display to be ready timed out.
- DLSDK_UNSUCCESSFUL on unknown error.

#### `dlsdk_display_clear`

```c
dlsdk_status dlsdk_display_clear(dlsdk_display_handle display);
```

Function used to clear the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `dlsdk_display_get_dpaux`

```c
dlsdk_dpaux_handle dlsdk_display_get_dpaux(dlsdk_display_handle display);
```

Function used to obtain a handle to the DP AUX channel associated with this display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.

**Returns:** A handle to the DP AUX channel, or NULL if the display is not
connected through DisplayPort.

#### `dlsdk_display_get_brightness`

```c
dlsdk_status dlsdk_display_get_brightness(dlsdk_display_handle display, uint8_t* brightness);
```

Function to get the brightness of the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `brightness` — Current brightness level (0-100)

**Returns:** DLSDK_SUCCESS on success, or an error code indicating the failure reason.

#### `dlsdk_display_set_brightness`

```c
dlsdk_status dlsdk_display_set_brightness(dlsdk_display_handle display, uint8_t brightness);
```

Function to set the brightness of the display.

**Parameters:**

- `display` — Display handle. Must be valid and non-NULL.
- `brightness` — Brightness level to set (0-100)

**Returns:** DLSDK_SUCCESS on success, or an error code indicating the failure reason.

### DP AUX API

Low-level access to the DisplayPort AUX channel, useful when direct AUX/I2C
access is required beyond the automatic display enumeration.

#### `dlsdk_dpaux_read`

```c
dlsdk_aux_status dlsdk_dpaux_read(dlsdk_dpaux_handle dpaux, uint32_t auxRegAddress, uint8_t* data, uint8_t length);
```

Reads data from native DP AUX registers using 16-byte burst transactions. Larger requests are serviced by chaining multiple burst reads into the provided buffer. The function waits for a response for up to 400 µs.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.
- `auxRegAddress` — DP register address.
- `data` — Buffer for incoming data.
- `length` — Data buffer length (0 <= length < 256).

**Returns:** The status response from the DP peer.

#### `dlsdk_dpaux_write`

```c
dlsdk_aux_status dlsdk_dpaux_write(dlsdk_dpaux_handle dpaux, uint32_t auxRegAddress, const uint8_t* data, uint8_t length);
```

Writes data to native DP AUX registers using 16-byte burst transactions. Larger requests are split into multiple burst writes from the provided buffer. The function waits for a response for up to 400 µs.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.
- `auxRegAddress` — DP register address.
- `data` — Buffer of write data.
- `length` — Data buffer length (0 <= length < 256).

**Returns:** The status response from the DP peer.

#### `dlsdk_dpaux_i2c_read`

```c
dlsdk_aux_status dlsdk_dpaux_i2c_read(dlsdk_dpaux_handle dpaux, uint8_t deviceAddress, uint8_t* data, uint8_t length, uint8_t mot, uint32_t timeoutMs);
```

Reads I2C data using an AUX transaction.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.
- `deviceAddress` — I2C device address (bits 6:0).
- `data` — Read data buffer.
- `length` — Data buffer length (0 <= length < 256).
- `mot` — A value of 1 indicates the middle of a transaction; a value of 0 terminates the I2C transaction with a stop.
- `timeoutMs` — Allowed timeout per transaction.

**Returns:** The status response from the DP peer.

#### `dlsdk_dpaux_i2c_write`

```c
dlsdk_aux_status dlsdk_dpaux_i2c_write(dlsdk_dpaux_handle dpaux, uint8_t deviceAddress, const uint8_t* data, uint8_t length, uint8_t mot, uint32_t timeoutMs);
```

Writes I2C data using an AUX transaction.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.
- `deviceAddress` — I2C device address (bits 6:0).
- `data` — Write data buffer.
- `length` — Data buffer length (0 <= length < 256).
- `mot` — A value of 1 indicates the middle of a transaction; a value of 0 terminates the I2C transaction with a stop.
- `timeoutMs` — Allowed timeout per transaction.

**Returns:** The status response from the DP peer.

#### `dlsdk_dpaux_i2c_write_and_read`

```c
dlsdk_aux_status dlsdk_dpaux_i2c_write_and_read(dlsdk_dpaux_handle dpaux, uint8_t deviceAddress, const uint8_t* writeData, uint8_t writeLength, uint8_t* readData, uint8_t readLength, uint32_t timeoutMs);
```

Writes and reads I2C data using an AUX transaction.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.
- `deviceAddress` — I2C device address (bits 6:0).
- `writeData` — Write data buffer.
- `writeLength` — Write data buffer length (0 <= length < 256).
- `readData` — Read data buffer.
- `readLength` — Read data buffer length (0 <= length < 256).
- `timeoutMs` — Allowed timeout per transaction.

**Returns:** The status response from the DP peer.

#### `dlsdk_dpaux_begin_exclusive_access`

```c
dlsdk_status dlsdk_dpaux_begin_exclusive_access(dlsdk_dpaux_handle dpaux, uint32_t timeoutMs);
```

Begins exclusive access to the DP AUX channel.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.
- `timeoutMs` — Maximum time to wait to acquire exclusive access, in milliseconds.

**Returns:** DLSDK_SUCCESS or the reason for failure.

#### `dlsdk_dpaux_end_exclusive_access`

```c
dlsdk_status dlsdk_dpaux_end_exclusive_access(dlsdk_dpaux_handle dpaux);
```

Ends exclusive access to the DP AUX channel.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS or the reason for failure.

#### `dlsdk_dpaux_detect`

```c
dlsdk_status dlsdk_dpaux_detect(dlsdk_dpaux_handle dpaux);
```

Detects whether a monitor is connected to the DP AUX channel and forces link
training.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL

**Returns:** DLSDK_SUCCESS if a monitor is detected, DLSDK_UNSUCCESSFUL_NO_MONITOR if no monitor is detected, or another error code on failure.

#### `dlsdk_dpaux_enable_virtual_hpd_polling`

```c
dlsdk_status dlsdk_dpaux_enable_virtual_hpd_polling(dlsdk_dpaux_handle dpaux);
```

Function used to enable polling for DP-connected monitors. Useful for detecting hotplug events on monitors that do not support HPD interrupts.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL

**Returns:** DLSDK_SUCCESS or reason for failure. If the device does not support virtual HPD polling, the function will return DLSDK_UNSUCCESSFUL.

#### `dlsdk_dpaux_disable_virtual_hpd_polling`

```c
dlsdk_status dlsdk_dpaux_disable_virtual_hpd_polling(dlsdk_dpaux_handle dpaux);
```

Function used to disable polling for DP-connected monitors.

**Parameters:**

- `dpaux` — DP AUX channel handle. Must be valid and non-NULL

**Returns:** DLSDK_SUCCESS or reason for failure. If the device does not support virtual HPD polling, the function will return DLSDK_UNSUCCESSFUL.