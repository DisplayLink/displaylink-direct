# C++ API Documentation

!!! info

    See [Minimal Example](minimal-example.md) for a minimal usage example that powers on a display and presents a solid-color frame.

The C++ helper classes are declared in `include/dlsdk/dlsdk.h`, in the
`dl::sdk` namespace. They are thin wrappers around the [C API](api-documentation.md)
and are provided as example code, built directly on top of the same
`dlsdk_*` functions documented there.

Objects are created in the following order: a `dl::sdk::System` is
initialised first, which is then used to enumerate `dl::sdk::DeviceHandle`
objects, which in turn expose their attached `dl::sdk::DisplayHandle`
objects. This page documents the API in that same order: System, Device,
Display, then DP AUX.

## Thread Safety

The API is thread-safe: it may be called concurrently from multiple threads, including on the same handle object (e.g. `dl::sdk::DeviceHandle`, `dl::sdk::DisplayHandle`, `dl::sdk::DpAux`), without external locking. Concurrent calls on the same handle are serialised internally, so a long-running call such as `DeviceHandle::updateFirmware()` will block other threads until it completes.

The following remain the caller's responsibility:

- `dl::sdk::System` construction and destruction are not thread-safe. Perform them from a single thread, and ensure no other API call is in progress when the `System` object is destroyed.
- All handles are owned by the DisplayLink Direct and are invalidated when the `System` object is destroyed; they must not be used after that point.
- Serialization of concurrent calls on the same handle is handled internally, but the order is time-dependent. Callers must still sequence dependent operations.
- A hotplug callback must not call `System::registerHotplugCallback()` or `System::unregisterHotplugCallback()`, as this will deadlock.

## Types

Custom types used across the C++ API wrapper.

| Type | Description |
| ---- | ----------- |
| `dl::sdk::Rect` | Alias for `dlsdk_rect`, an abstract representation of a rectangular area |
| `dl::sdk::Config` | Configuration struct used to construct a `dl::sdk::System` with custom options |
| `dl::sdk::HotplugEventData` | Data provided to hotplug callbacks when a hotplug event occurs |
| `dl::sdk::CallbackHandle` | Alias for `dlsdk_hotplug_callback_handle`, used to unregister a hotplug callback |
| `dl::sdk::CallbackFn` | `std::function` signature used for hotplug callbacks |
| `dl::sdk::DpAux` | Wrapper for a DP AUX channel |
| `dl::sdk::DpAuxExclusiveAccess` | RAII helper for exclusive access to a DP AUX channel |
| `dl::sdk::System` | Wrapper for system-level initialization, enumeration, and hotplug callbacks |
| `dl::sdk::DeviceHandle` | Wrapper for an attached device |
| `dl::sdk::DisplayHandle` | Wrapper for a display attached to a device |

#### `dl::sdk::Rect`

```cpp
using Rect = dlsdk_rect;
```

Alias for `dlsdk_rect`, an abstract representation of a rectangular area, used for display resolution and size information: `width`, `height`.

#### `dl::sdk::Config`

```cpp
struct Config final
{
  Config() = default;
  Config(const char* firmwarePath, uint32_t flags = 0);

  std::string firmwarePath;
  uint32_t flags = 0;
};
```

Configuration struct used to construct a `dl::sdk::System` with custom options.

| Field | Description |
| ----- | ----------- |
| `firmwarePath` | Firmware directory path used to update the firmware automatically if required. Leave empty to disable automatic firmware updates. If automatic firmware updates are disabled, the user should ensure that the firmware on the device is compatible with the DisplayLink Direct version; otherwise, the device may not function properly or at all. |
| `flags` | Combination of `dlsdk_config_flags` values. Set `DLSDK_CONFIG_FLAG_EMBEDDED_MODE` to enable embedded mode, in which the DisplayLink Direct will not use dynamic memory at runtime. |

#### `dl::sdk::HotplugEventData`

```cpp
struct HotplugEventData
{
  uint32_t version;
  dlsdk_hotplug_event event;
  DeviceHandle device;
  std::string device_id;
  DisplayHandle display;
  std::string display_id;
};
```

| Field | Description |
| ----- | ----------- |
| `version` | See `dlsdk_hotplug_event_data.version`. |
| `event` | See `dlsdk_hotplug_event_data.event`. |
| `device` | See `dlsdk_hotplug_event_data.device`. |
| `device_id` | See `dlsdk_hotplug_event_data.device_id`. |
| `display` | See `dlsdk_hotplug_event_data.display`. |
| `display_id` | See `dlsdk_hotplug_event_data.display_id`. |

#### `dl::sdk::CallbackHandle`

```cpp
using CallbackHandle = dlsdk_hotplug_callback_handle;
```

Alias for `dlsdk_hotplug_callback_handle`, the opaque handle returned by `System::registerHotplugCallback()` and passed to `System::unregisterHotplugCallback()`.

#### `dl::sdk::CallbackFn`

```cpp
using CallbackFn = std::function<void(const HotplugEventData&)>;
```

`std::function` signature used for hotplug callbacks registered with `System::registerHotplugCallback()`.

## System API

### `dl::sdk::System`

Class methods not related to a particular device or display.

```cpp
struct System final
{
  System();
  System(const Config& config);
  ~System();

  dlsdk_status initialised();
  std::vector<DeviceHandle> getDevices();
  dlsdk_status registerHotplugCallback(const CallbackFn& cb, CallbackHandle* outHandle);
  dlsdk_status unregisterHotplugCallback(CallbackHandle handle);
};
```

#### `System()`

Function used to initialise the internals of the DisplayLink Direct.

**Details:** This method must be called by the user prior to invoking any other functionality. In OO parlance, it can be thought of as a DisplayLink Direct constructor.

#### `System(const Config& config)`

Function used to initialise the DisplayLink Direct with a specific configuration.

**Details:** This method must be called by the user prior to invoking any other functionality. In OO parlance, it can be thought of as a DisplayLink Direct constructor.

**Parameters:**
- `config` — The configuration structure containing the firmware path and other settings. See `Config` for details.

#### `~System()`

Function used to clean up the SDK internals when a user no longer requires its use.

**Details:** This method must be called before the application exits. In OO
parlance, it can be thought of as an DisplayLink Direct destructor.

#### `initialised()`

```cpp
dlsdk_status initialised();
```

Returns the status from `dlsdk_initialise()` or
`dlsdk_initialise_with_config()` that was called by the constructor.

**Returns:** DLSDK_SUCCESS on success, otherwise the initialisation error code.

#### `getDevices()`

```cpp
std::vector<DeviceHandle> getDevices();
```

Function used to obtain the attached devices.

**Returns:** A `std::vector` of `DeviceHandle` objects, one for each device currently attached.

#### `registerHotplugCallback()`

```cpp
dlsdk_status registerHotplugCallback(const CallbackFn& cb, CallbackHandle* outHandle);
```

Registers a hotplug callback function.

**Details:** This function registers a callback to be invoked when a hotplug event occurs and returns a registration handle for later unregistration. Only one callback can be registered, and registering a new callback will overwrite the previous one. The callback will be invoked for all devices and displays currently connected and for all subsequent hotplug events from the moment the callback is registered. All callbacks are invoked on an internal DisplayLink Direct worker thread. When a system is destroyed, all hotplug callbacks will be automatically unregistered and their handles invalidated.

**Parameters:**
- `cb` — the function to be invoked when a hotplug event occurs. Must be non-NULL.
- `outHandle` — Callback registration handle. Must be non-NULL. On success, receives a non-NULL handle.

**Returns:** DLSDK_SUCCESS on success, otherwise an error code.

#### `unregisterHotplugCallback()`

```cpp
dlsdk_status unregisterHotplugCallback(CallbackHandle handle);
```

Unregisters a hotplug callback.

**Details:** You must not register or unregister callbacks from within a callback.
The callback will not be invoked after this function returns successfully. If
the callback is currently running, this function blocks until it completes
before unregistering and returning.

**Parameters:**
- `handle` — The callback registration handle returned during registration.

**Returns:** DLSDK_SUCCESS on success, DLSDK_INVALID_HANDLE if `handle` is unknown or already unregistered, otherwise an error code.

## Device API

### `dl::sdk::DeviceHandle`

Class methods for manipulating or controlling attached devices.

```cpp
struct DeviceHandle final
{
  dlsdk_status restart();
  std::string id() const;
  std::string firmwareVersion() const;
  std::vector<DisplayHandle> getDisplays();
  DpAux getDpAux(unsigned int output);
  dlsdk_status updateFirmware(const char* firmwarePath);
};
```

#### `restart()`

```cpp
dlsdk_status restart();
```

Function used to restart a device.

**Details:** On calling this function, the device will be restarted. The device and display
handles remain valid after restart, but the rendering pipeline must be reestablished and any
displays must be powered on again. In embedded mode, the device and displays will no longer be
accessible through their handles and the user must reinitialise the system to obtain new handles.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `id()`

```cpp
std::string id() const;
```

Function used to obtain the unique identifier of the device.

**Details:** Based on the device's USB serial number, so the identifier persists across unplugging and power cycling. Example: "USB_6015-10096418", where 6015 is the USB product ID (in hex) and 10096418 is the serial number. The DisplayLink USB vendor ID (0x17e9) is not included.

**Returns:** A string containing the unique device identifier.

#### `firmwareVersion()`

```cpp
std::string firmwareVersion() const;
```

Function used to obtain the firmware version of the device.

**Returns:** A string containing the firmware version of the device.

#### `getDisplays()`

```cpp
std::vector<DisplayHandle> getDisplays();
```

Function used to obtain the attached displays for a given device.

**Details:** Display handles are owned by their device and must not be freed by the caller; they remain valid until the DisplayLink Direct system is torn down. In embedded mode, the device and displays become inaccessible through their handles after the device is restarted.

**Returns:** A `std::vector` of `DisplayHandle` objects, one for each display currently attached to the device.

#### `getDpAux()`

```cpp
DpAux getDpAux(unsigned int output);
```

Function used to obtain a handle to the DP AUX channel available on a given device.

**Details:** Some devices may not have a DP AUX channel, in which case this
function returns an invalid `DpAux`. This function is useful when low-level
access to the DP AUX channel is required and the display is not enumerated
automatically. Otherwise, obtain the DP AUX channel through
`DisplayHandle::getDpAux()`.

**Parameters:**
- `output` — The output index for which to obtain the DP AUX channel handle.

**Returns:** A `DpAux` object, or an invalid object if the device does not
support the channel, the output is not a DisplayPort output, or the output does
not exist.

#### `updateFirmware()`

```cpp
dlsdk_status updateFirmware(const char* firmwarePath);
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
- `firmwarePath` — The path to the firmware directory provided by DisplayLink. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS on success; otherwise, an error code.
DLSDK_UNSUCCESSFUL_NO_DEVICE is returned if the device is not found within 15
seconds after the firmware update.

See [Firmware Update](../getting-started/firmware-update.md) for more on
firmware-related workflows.

## Display API

### `dl::sdk::DisplayHandle`

Class methods for manipulating/controlling displays attached to a device.

```cpp
struct DisplayHandle final
{
  std::string id() const;
  Rect size() const;
  dlsdk_status edid(uint8_t* data, uint32_t* len) const;
  dlsdk_status getCapabilities(dlsdk_display_capabilities* capabilities) const;
  dlsdk_status modes(dlsdk_display_mode* modes, unsigned int* count) const;
  dlsdk_status preferredMode(dlsdk_display_mode* mode) const;
  dlsdk_status powerOn();
  dlsdk_status powerOn(const dlsdk_display_power_on_options& options);
  dlsdk_status powerOff();
  dlsdk_status show(const dlsdk_frame& frame);
  dlsdk_status waitOnShow(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
  dlsdk_status clear();
  DpAux getDpAux();
  dlsdk_status getBrightness(uint8_t* brightness);
  dlsdk_status setBrightness(uint8_t brightness);
};
```

#### `id()`

```cpp
std::string id() const;
```

Function used to obtain the unique identifier of the display.

**Details:** Identifies a single output on a device; a device with multiple outputs (e.g. up to 4 HDMI connectors) exposes one identifier per output. Example: "USB_6015-10096418^0", "USB_6015-10096418^1", "USB_6015-10096418^2", "USB_6015-10096418^3", where 6015 is the USB product ID (in hex), 10096418 is the serial number, and the suffix after ^ is the output index. The DisplayLink USB vendor ID (0x17e9) is not included.

**Returns:** A string containing the device and head identifiers.

#### `size()`

```cpp
Rect size() const;
```

Function used to obtain the resolution of the display.

**Returns:** The resolution of the display.

#### `edid()`

```cpp
dlsdk_status edid(uint8_t* data, uint32_t* len) const;
```

Function used to obtain the EDID of the display.

**Parameters:**
- `data` — Buffer to store the EDID data. If null, the function will return the required buffer size in len.
- `len` — Length of the data buffer. On input, should be set to the size of the data buffer. On output, will be set to the actual length of the EDID data written to the buffer, or the required buffer size if buffer is null. If the buffer is too small, the data will be truncated.

**Returns:** DLSDK_SUCCESS if the EDID was obtained successfully, or the reason
for failure otherwise.

#### `getCapabilities()`

```cpp
dlsdk_status getCapabilities(dlsdk_display_capabilities* capabilities) const;
```

Function used to obtain signal mode and pixel format capabilities for the display.

**Details:** Initialise `capabilities` with `DLSDK_DISPLAY_CAPABILITIES_INIT(&capabilities)` before calling this function; the macro sets `capabilities->size` to `sizeof(dlsdk_display_capabilities)`. The `signalModeMask` uses the value of `dlsdk_display_signal_mode` as its bit index. The `pixelFormatMask` uses the value of `dlsdk_pixel_format` as its bit index.

**Parameters:**
- `capabilities` — Buffer to store the display capabilities. Must be valid and non-NULL.

**Returns:** DLSDK_SUCCESS if the capabilities were obtained successfully, or the reason for failure otherwise.

#### `modes()`

```cpp
dlsdk_status modes(dlsdk_display_mode* modes, unsigned int* count) const;
```

Function used to retrieve a list of supported display modes for the display.

**Parameters:**
- `modes` — Buffer to store the display modes. If null, the function will return the required buffer size in count.
- `count` — Number of display modes that can be stored in the modes buffer. On input, should be set to the size of the modes buffer. On output, will be set to the actual number of display modes written to the buffer, or the required buffer size if buffer is null.

**Returns:** DLSDK_SUCCESS if the display modes were obtained successfully, or
the reason for failure otherwise.

#### `preferredMode()`

```cpp
dlsdk_status preferredMode(dlsdk_display_mode* mode) const;
```

Function used to obtain the preferred display mode for the display.

**Parameters:**
- `mode` — Buffer to store the preferred display mode.

**Returns:** DLSDK_SUCCESS if the preferred display mode was obtained
successfully, or the reason for failure otherwise.

#### `powerOn()`

```cpp
dlsdk_status powerOn();
```

Function used to power on the display in its preferred mode and SDR signal mode.

**Details:** This is equivalent to calling `powerOn(const dlsdk_display_power_on_options& options)` with an options structure initialised by `DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)`.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `powerOn(const dlsdk_display_power_on_options& options)`

```cpp
dlsdk_status powerOn(const dlsdk_display_power_on_options& options);
```

Function used to power on the display with custom options.

**Details:** The display is powered on using the preferred display mode when `options.mode` and `options.timing` are null. When `options.signalMode` is `DLSDK_DISPLAY_SIGNAL_MODE_HDR10`, you must use `DLSDK_PIXEL_FORMAT_RGB10` or `DLSDK_PIXEL_FORMAT_XBGR16F` in `DisplayHandle::show()`.

**Parameters:**
- `options` — Power-on options. Must be valid. Initialise it with `DLSDK_DISPLAY_POWER_ON_OPTIONS_INIT(&options)` before setting optional fields. The mode and timing pointers are mutually exclusive.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `powerOff()`

```cpp
dlsdk_status powerOff();
```

Function used to power off the display.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `show()`

```cpp
dlsdk_status show(const dlsdk_frame& frame);
```

Function to present the contents of a frame on the display.

**Details:** This is a non-blocking call and returns before the pixels are
displayed. To receive notification when the pixels have been displayed, call
`DisplayHandle::waitOnShow()`. The frame can describe either a full-screen
update or a partial update using dirty rectangles. Its pixel buffer must
describe the entire display, and its optional static HDR metadata is applied
synchronously with the frame.

**Parameters:**
- `frame` — Frame descriptor containing the pixel format, pixel buffer, optional stride override, optional dirty rectangles, and optional static HDR metadata. Initialise it with `DLSDK_FRAME_INIT(&frame, FORMAT, PIXELS, PIXELS_SIZE)` before calling this method. The pixel buffer must be non-null and large enough for the display height and effective stride; a 64-byte alignment of the pixel buffer address and stride is preferred. When the display is powered on in HDR10 mode, the frame format must provide at least 10 bits per channel of source precision and be supported by the display capabilities.

**Returns:** DLSDK_SUCCESS if the frame was successfully submitted for rendering, or an error code on failure. The caller should check the render and frame transmission status with `DisplayHandle::waitOnShow()`.

#### `waitOnShow()`

```cpp
dlsdk_status waitOnShow(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
```

Function used to block for notification that pixels have been displayed after calling `DisplayHandle::show()`, with an optional timeout.

**Parameters:**
- `timeout` — Maximum time to wait. Defaults to waiting indefinitely (`std::chrono::milliseconds::max()`).

**Returns:**

- DLSDK_SUCCESS when the pixels have been displayed.
- DLSDK_UNSUCCESSFUL_NO_MONITOR when display is not ready.
- DLSDK_BUSY when the device aborted a previously submitted frame due to resource constraints. The caller should retry `DisplayHandle::show()` with the frame to be displayed, which will typically involve more aggressive compression, or proceed with a new frame if the previous frame is no longer relevant.
- DLSDK_TIMEOUT if waiting for the display to be ready timed out.
- DLSDK_UNSUCCESSFUL on unknown error.

#### `clear()`

```cpp
dlsdk_status clear();
```

Function used to clear the display.

**Returns:** DLSDK_SUCCESS or an appropriate error code on failure.

#### `getDpAux()`

```cpp
DpAux getDpAux();
```

Function used to obtain a handle to the DP AUX channel associated with this display.

**Returns:** A `DpAux` object, or an invalid object if the display is not
connected through DisplayPort.

#### `getBrightness()`

```cpp
dlsdk_status getBrightness(uint8_t* brightness);
```

Function used to obtain the current display brightness.

**Parameters:**
- `brightness` — Receives the display brightness in the range 0 to 100.

**Returns:** DLSDK_SUCCESS on success, or an appropriate error code on failure.

#### `setBrightness()`

```cpp
dlsdk_status setBrightness(uint8_t brightness);
```

Function used to set the display brightness.

**Parameters:**
- `brightness` — Display brightness in the range 0 to 100.

**Returns:** DLSDK_SUCCESS on success, or an appropriate error code on failure.

## DP AUX API

Low-level access to the DisplayPort AUX channel, useful when direct AUX/I2C
access is required beyond the automatic display enumeration.

### `dl::sdk::DpAux`

Class methods for the DP AUX channel.

```cpp
class DpAux final
{
public:
  explicit DpAux(dlsdk_dpaux_handle dpaux);

  dlsdk_aux_status auxReadData(uint32_t auxRegAddress, uint8_t* data, uint8_t length);
  dlsdk_aux_status auxWriteData(uint32_t auxRegAddress, const uint8_t* data, uint8_t length);
  dlsdk_aux_status auxI2cReadData(uint8_t deviceAddress, uint8_t* data, uint8_t length, bool mot, std::chrono::milliseconds timeout);
  dlsdk_aux_status auxI2cWriteData(uint8_t deviceAddress, const uint8_t* data, uint8_t length, bool mot, std::chrono::milliseconds timeout);
  dlsdk_aux_status auxI2cWriteAndReadData(uint8_t deviceAddress, const uint8_t* writeData, uint8_t writeLength, uint8_t* readData, uint8_t readLength, std::chrono::milliseconds timeout);
  dlsdk_status beginExclusiveAccess(std::chrono::milliseconds timeout);
  dlsdk_status endExclusiveAccess();
  dlsdk_status detect();
  dlsdk_status enableVirtualHpdPolling();
  dlsdk_status disableVirtualHpdPolling();
};
```

#### `auxReadData()`

```cpp
dlsdk_aux_status auxReadData(uint32_t auxRegAddress, uint8_t* data, uint8_t length);
```

Reads data from native DP AUX registers using 16-byte burst transactions. Larger
requests are serviced by chaining multiple burst reads into the provided buffer.
The function waits for a response for up to 400 µs.

**Parameters:**
- `auxRegAddress` — DP register address.
- `data` — Buffer for incoming data.
- `length` — Data buffer length (0 <= length < 256).

**Returns:** The status response from the DP peer.

#### `auxWriteData()`

```cpp
dlsdk_aux_status auxWriteData(uint32_t auxRegAddress, const uint8_t* data, uint8_t length);
```

Writes data to native DP AUX registers using 16-byte burst transactions. Larger
requests are split into multiple burst writes from the provided buffer. The
function waits for a response for up to 400 µs.

**Parameters:**
- `auxRegAddress` — DP register address.
- `data` — Buffer of write data.
- `length` — Data buffer length (0 <= length < 256).

**Returns:** The status response from the DP peer.

#### `auxI2cReadData()`

```cpp
dlsdk_aux_status auxI2cReadData(uint8_t deviceAddress, uint8_t* data, uint8_t length, bool mot, std::chrono::milliseconds timeout);
```

Reads I2C data using an AUX transaction.

**Parameters:**
- `deviceAddress` — I2C device address (bits 6:0).
- `data` — Read data buffer.
- `length` — Data buffer length (0 <= length < 256).
- `mot` — A value of 1 indicates the middle of a transaction; a value of 0 terminates the I2C transaction with a stop.
- `timeout` — Allowed timeout per transaction.

**Returns:** The status response from the DP peer.

#### `auxI2cWriteData()`

```cpp
dlsdk_aux_status auxI2cWriteData(uint8_t deviceAddress, const uint8_t* data, uint8_t length, bool mot, std::chrono::milliseconds timeout);
```

Writes I2C data using an AUX transaction.

**Parameters:**
- `deviceAddress` — I2C device address (bits 6:0).
- `data` — Write data buffer.
- `length` — Data buffer length (0 <= length < 256).
- `mot` — A value of 1 indicates the middle of a transaction; a value of 0 terminates the I2C transaction with a stop.
- `timeout` — Allowed timeout per transaction.

**Returns:** The status response from the DP peer.

#### `auxI2cWriteAndReadData()`

```cpp
dlsdk_aux_status auxI2cWriteAndReadData(uint8_t deviceAddress, const uint8_t* writeData, uint8_t writeLength, uint8_t* readData, uint8_t readLength, std::chrono::milliseconds timeout);
```

Writes and reads I2C data using an AUX transaction.

**Parameters:**
- `deviceAddress` — I2C device address (bits 6:0).
- `writeData` — Write data buffer.
- `writeLength` — Write data buffer length (0 <= length < 256).
- `readData` — Read data buffer.
- `readLength` — Read data buffer length (0 <= length < 256).
- `timeout` — Allowed timeout per transaction.

**Returns:** The status response from the DP peer.

#### `beginExclusiveAccess()`

```cpp
dlsdk_status beginExclusiveAccess(std::chrono::milliseconds timeout);
```

Begins exclusive access to the DP AUX channel.

**Parameters:**
- `timeout` — Maximum time to wait to acquire exclusive access.

**Returns:** DLSDK_SUCCESS or the reason for failure.

#### `endExclusiveAccess()`

```cpp
dlsdk_status endExclusiveAccess();
```

Ends exclusive access to the DP AUX channel.

**Returns:** DLSDK_SUCCESS or the reason for failure.

#### `detect()`

```cpp
dlsdk_status detect();
```

Detects whether a monitor is connected to the DP AUX channel and forces link
training.

**Returns:** DLSDK_SUCCESS if a monitor is detected, DLSDK_UNSUCCESSFUL_NO_MONITOR if no monitor
is detected, or another error code on failure.

#### `enableVirtualHpdPolling()`

```cpp
dlsdk_status enableVirtualHpdPolling();
```

Function used to enable polling for DP-connected monitors. Useful for detecting hotplug events on monitors that do not support HPD interrupts.

**Returns:** DLSDK_SUCCESS or reason for failure. If the device does not support virtual HPD polling, the function will return DLSDK_UNSUCCESSFUL.

#### `disableVirtualHpdPolling()`

```cpp
dlsdk_status disableVirtualHpdPolling();
```

Function used to disable polling for DP-connected monitors.

**Returns:** DLSDK_SUCCESS or reason for failure. If the device does not support virtual HPD polling, the function will return DLSDK_UNSUCCESSFUL.

### `dl::sdk::DpAuxExclusiveAccess`

RAII helper that begins exclusive access to a `DpAux` channel on construction and ends it on destruction, if it was successfully acquired.

```cpp
class DpAuxExclusiveAccess final
{
public:
  explicit DpAuxExclusiveAccess(DpAux* dpaux, std::chrono::milliseconds timeout);
  ~DpAuxExclusiveAccess();

  DpAuxExclusiveAccess(const DpAuxExclusiveAccess&) = delete;
  DpAuxExclusiveAccess& operator=(const DpAuxExclusiveAccess&) = delete;

  dlsdk_status status() const;
};
```

#### `DpAuxExclusiveAccess()`

```cpp
explicit DpAuxExclusiveAccess(DpAux* dpaux, std::chrono::milliseconds timeout);
```

Calls `DpAux::beginExclusiveAccess()` on the given `dpaux` with the given `timeout`, storing the resulting status for later retrieval via `status()`.

**Parameters:**
- `dpaux` — Pointer to the `DpAux` channel to acquire exclusive access to.
- `timeout` — Maximum time to wait to acquire exclusive access.

#### `~DpAuxExclusiveAccess()`

```cpp
~DpAuxExclusiveAccess();
```

Calls `DpAux::endExclusiveAccess()` on the associated `dpaux`, if exclusive access was successfully acquired by the constructor.

#### `status()`

```cpp
dlsdk_status status() const;
```

**Returns:** The status returned by `DpAux::beginExclusiveAccess()` when this object was constructed.
