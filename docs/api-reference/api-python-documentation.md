# Python API Documentation

!!! info

    See [Minimal Example](minimal-example.md) for a minimal usage example that powers on a display and presents a solid-color frame.

The Python bindings are a thin `pybind11` wrapper around the same
[C API](api-documentation.md) declared in `include/dlsdk/dlsdk.h`, exposed as
the `dlsdk` module.

Objects are created in the following order: the module-level system
functions are called first, which are then used to enumerate `dlsdk.Device`
objects, which in turn expose their attached `dlsdk.Display` objects. This
page documents the API in that same order: System, Device, Display, then DP
AUX.

## Thread Safety

The API is thread-safe: it may be called concurrently from multiple threads, including on the same object (e.g. `dlsdk.Device`, `dlsdk.Display`, `dlsdk.DpAux`), without external locking. Concurrent calls on the same object are serialised internally, so a long-running call such as `dlsdk.Device.update_firmware()` will block other threads until it completes.

The following remain the caller's responsibility:

- Module-level system initialization and teardown are not thread-safe. Call them from a single thread, and ensure no other API call is in progress on any thread when the system is torn down.
- All objects are owned by the DisplayLink Direct and are invalidated when the system is torn down; they must not be used after that point.
- Serialization of concurrent calls on the same object is handled internally, but the order is time-dependent. Callers must still sequence dependent operations.
- A hotplug callback must not call `dlsdk.register_hotplug_callback()` or `dlsdk.unregister_hotplug_callback()`, as this will deadlock.

## Types

Custom types used across the Python API.

| Type | Description |
| ---- | ----------- |
| `dlsdk.Config` | Configuration for initialising DisplayLink Direct. |
| `dlsdk.Rect` | An abstract representation of a rectangular area, used for display resolution and size information. |
| `dlsdk.DirtyRect` | A rectangular area of a pixel buffer, relative to its top-left corner. |
| `dlsdk.DisplayMode` | A display mode represents a specific resolution and refresh rate that a display can operate at. |
| `dlsdk.DisplayTiming` | Detailed video timing parameters used to power on a display with a fully custom timing, rather than one of the modes reported by `dlsdk.Display.modes()`. |
| `dlsdk.DisplayCapabilities` | Display capabilities relevant to signal modes and pixel formats. |
| `dlsdk.HdrSmpte2086` | SMPTE ST 2086 mastering display color volume metadata. |
| `dlsdk.HdrCta8613` | CTA-861.3 content light level metadata. |
| `dlsdk.Hdr10Metadata` | HDR10 metadata containing SMPTE 2086 and CTA-861.3 data. |
| `dlsdk.HdrStaticMetadata` | Static HDR metadata associated with a frame. |
| `dlsdk.dlsdk_display_signal_mode` | Display signal mode used by the display signal. |
| `dlsdk.dlsdk_hdr_metadata_flags` | Flags identifying the static HDR metadata format associated with a frame. |
| `dlsdk.dlsdk_status` | API method status return codes. |
| `dlsdk.dlsdk_pixel_format` | API-supported pixel formats. |
| `dlsdk.dlsdk_aux_status` | DP AUX channel return codes. |
| `dlsdk.dlsdk_hotplug_event` | Hotplug event types. |
| `dlsdk.HotplugEventData` | Data provided to hotplug callbacks when a hotplug event occurs. |

#### `dlsdk.Config`

```python
Config(firmware_path: str = ..., embedded_mode: bool = False)
```

Configuration for initialising DisplayLink Direct.

**Parameters:**

- `firmware_path` — Path to the firmware directory used by the SDK.
- `embedded_mode` — When `True`, the SDK avoids dynamic memory allocations at runtime.

**Notes:** The default constructor discovers the firmware path from the active Python site-packages installation and falls back to an empty path if no matching package directory is found.

#### `dlsdk.Rect`

An abstract representation of a rectangular area, used for display resolution and size information.

| Field | Description |
| ----- | ----------- |
| `width` | Width, in pixels. |
| `height` | Height, in pixels. |

#### `dlsdk.DirtyRect`

```python
DirtyRect()
DirtyRect(x: int, y: int, width: int, height: int)
```

A rectangular area of a pixel buffer, relative to its top-left corner.

| Field | Description |
| ----- | ----------- |
| `x` | Horizontal offset of the rectangle from the left edge of the pixel buffer, in pixels. |
| `y` | Vertical offset of the rectangle from the top edge of the pixel buffer, in pixels. |
| `width` | Width of the rectangle, in pixels. |
| `height` | Height of the rectangle, in pixels. |

`dlsdk.MAX_DIRTY_RECTS` (128) is the maximum number of dirty rectangles accepted by `dlsdk.Display.show()`.

#### `dlsdk.DisplayMode`

A display mode represents a specific resolution and refresh rate that a display can operate at.

| Field | Description |
| ----- | ----------- |
| `resolution` | The mode resolution, as a `dlsdk.Rect`. |
| `refresh_rate_hz` | The mode refresh rate, in Hz. |

#### `dlsdk.DisplayTiming`

```python
DisplayTiming()
```

Detailed video timing parameters used to power on a display with a fully custom timing, rather than one of the modes reported by `dlsdk.Display.modes()`.

| Field | Description |
| ----- | ----------- |
| `horizontal_resolution` | Active horizontal resolution, in pixels. |
| `vertical_resolution` | Active vertical resolution, in pixels. |
| `refresh_rate_milli_hz` | Refresh rate, in milli-Hertz (e.g. 60000 for 60Hz). |
| `h_total` | Total horizontal line time, in pixels. |
| `v_total` | Total vertical frame time, in lines. |
| `h_start` | Offset from the start of the line to the start of active video, in pixels (hsync width + back porch). |
| `v_start` | Offset from the start of the frame to the start of active video, in lines (vsync width + back porch). |
| `h_sync_width` | Horizontal sync pulse width, in pixels. |
| `v_sync_width` | Vertical sync pulse width, in lines. |
| `h_sync_polarity_positive` | Non-zero if the horizontal sync pulse is active-high. |
| `v_sync_polarity_positive` | Non-zero if the vertical sync pulse is active-high. |

#### `dlsdk.DisplayCapabilities`

```python
DisplayCapabilities()
```

Display capabilities relevant to signal modes and pixel formats.

| Field | Description |
| ----- | ----------- |
| `size` | Size of this structure. |
| `pixel_format_mask` | Supported `dlsdk.dlsdk_pixel_format` values. |
| `signal_mode_mask` | Supported `dlsdk.dlsdk_display_signal_mode` values. |

#### `dlsdk.HdrSmpte2086`

```python
HdrSmpte2086()
```

SMPTE ST 2086 mastering display color volume metadata.

**Details:** Chromaticity values are normalised CIE 1931 x/y coordinates in the range [0, 1]. Luminance values are in cd/m^2 (nits).

| Field | Description |
| ----- | ----------- |
| `red_x` | Red primary x coordinate. |
| `red_y` | Red primary y coordinate. |
| `green_x` | Green primary x coordinate. |
| `green_y` | Green primary y coordinate. |
| `blue_x` | Blue primary x coordinate. |
| `blue_y` | Blue primary y coordinate. |
| `white_x` | White point x coordinate. |
| `white_y` | White point y coordinate. |
| `max_luminance` | Maximum mastering luminance in cd/m^2. |
| `min_luminance` | Minimum mastering luminance in cd/m^2. |

#### `dlsdk.HdrCta8613`

```python
HdrCta8613()
```

CTA-861.3 content light level metadata.

**Details:** Light-level values are in cd/m^2 (nits).

| Field | Description |
| ----- | ----------- |
| `max_content_light_level` | Maximum content light level. |
| `max_frame_average_light_level` | Maximum frame-average light level. |

#### `dlsdk.Hdr10Metadata`

```python
Hdr10Metadata()
```

| Field | Description |
| ----- | ----------- |
| `smpte2086` | SMPTE 2086 mastering display color volume metadata. |
| `cta861_3` | CTA-861.3 content light level metadata. |

#### `dlsdk.HdrStaticMetadata`

```python
HdrStaticMetadata()
```

Static HDR metadata associated with a frame.

| Field | Description |
| ----- | ----------- |
| `size` | Size of this structure; set when the metadata is initialised. |
| `flags` | Combination of `dlsdk.dlsdk_hdr_metadata_flags` values. |
| `hdr10` | Complete HDR10 payload when its flag is set. |

#### `dlsdk.dlsdk_display_signal_mode`

Display signal mode used by the display signal.

| Value | Description |
| ----- | ----------- |
| `DLSDK_DISPLAY_SIGNAL_MODE_SDR` | Standard dynamic range signal. |
| `DLSDK_DISPLAY_SIGNAL_MODE_HDR10` | HDR10 signal. |

#### `dlsdk.dlsdk_hdr_metadata_flags`

Flags identifying the static HDR metadata format associated with a frame.

| Value | Description |
| ----- | ----------- |
| `DLSDK_HDR_METADATA_FLAG_NONE` | No static HDR metadata. |
| `DLSDK_HDR_METADATA_HDR10` | HDR10 static metadata. |

#### `dlsdk.dlsdk_status`

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
| `DLSDK_BUSY` | The device aborted a previously submitted frame due to resource constraints; retry `display.show()`. |

#### `dlsdk.dlsdk_pixel_format`

API-supported pixel formats.

| Value | Bytes per pixel |
| ----- | ---------------- |
| `DLSDK_PIXEL_FORMAT_XRGB` | 4 |
| `DLSDK_PIXEL_FORMAT_XBGR` | 4 |
| `DLSDK_PIXEL_FORMAT_RGB565` | 2 |
| `DLSDK_PIXEL_FORMAT_RGB` | 3 |
| `DLSDK_PIXEL_FORMAT_RGB10` | 4 |
| `DLSDK_PIXEL_FORMAT_XBGR16F` | 8 |

`DLSDK_PIXEL_FORMAT_XRGB` and `DLSDK_PIXEL_FORMAT_XBGR` are recommended for maximum compatibility and performance.

#### `dlsdk.dlsdk_aux_status`

DP AUX channel return codes.

| Value | Hex | Description |
| ----- | --- | ----------- |
| `DLSDK_AUX_ACK` | 0x0 | Standard DP response code |
| `DLSDK_AUX_NATIVE_NAK` | 0x1 | Standard DP response code |
| `DLSDK_AUX_NATIVE_DEFER` | 0x2 | Standard DP response code |
| `DLSDK_AUX_TIMEOUT` | 0x10 | Timeout, no response from downstream |
| `DLSDK_AUX_ERROR` | 0x11 | Error detected by DP hardware |
| `DLSDK_AUX_DETACHED` | 0x12 | Sink connection no longer present |

#### `dlsdk.dlsdk_hotplug_event`

Hotplug event types: `DLSDK_HOTPLUG_EVENT_DEVICE_ARRIVED`, `DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED`, `DLSDK_HOTPLUG_EVENT_DISPLAY_ARRIVED`, `DLSDK_HOTPLUG_EVENT_DISPLAY_REMOVED`.

#### `dlsdk.HotplugEventData`

Data provided to hotplug callbacks when a hotplug event occurs.

| Field | Description |
| ----- | ----------- |
| `version` | Struct version set by the SDK. |
| `event` | The `dlsdk.dlsdk_hotplug_event` that occurred. |
| `device` | The `dlsdk.Device` the event relates to, for device events. |
| `device_id` | The ID of the device to which the event relates. |
| `display` | The `dlsdk.Display` the event relates to, for display events. |
| `display_id` | The ID of the display to which the event relates. |

## System API

Module-level functions not related to a particular device or display.

#### `dlsdk.create_system`

```python
create_system(config: dlsdk.Config = dlsdk.Config()) -> dlsdk.dlsdk_status
```

Initialises DisplayLink Direct with the given configuration.

#### `dlsdk.delete_system`

```python
delete_system() -> None
```

Finish DisplayLink Direct and release all resources.

#### `dlsdk.get_devices`

```python
get_devices() -> list[dl::sdk::DeviceHandle]
```

Gets the enumerated DL devices

#### `dlsdk.register_hotplug_callback`

```python
register_hotplug_callback(callback: collections.abc.Callable) -> tuple
```

Registers a hotplug callback. Returns dlsdk_status and a registration handle.

#### `dlsdk.unregister_hotplug_callback`

```python
unregister_hotplug_callback(handle: typing_extensions.CapsuleType) -> dlsdk.dlsdk_status
```

Unregisters the previously registered hotplug callback using the given registration handle.

## Device API

### `dlsdk.Device`

Methods for manipulating/controlling attached devices.

#### `id`

```python
id(self: dlsdk.Device) -> str
```

String representation of the device identity

**Details:** Based on the device's USB serial number, so the identifier persists across unplugging and power cycling. Example: "USB_6015-10096418", where 6015 is the USB product ID (in hex) and 10096418 is the serial number. The DisplayLink USB vendor ID (0x17e9) is not included.

**Returns:** A string containing the unique device identifier.

#### `firmware_version`

```python
firmware_version(self: dlsdk.Device) -> str
```

String representation of the device firmware version

#### `get_displays`

```python
get_displays(self: dlsdk.Device) -> list[dl::sdk::DisplayHandle]
```

Returns the enumerated displays

**Details:** Display handles are owned by their device and must not be freed by the caller; they remain valid until the associated device handle is restarted with `restart()` or the system is deleted with `delete_system()`.

**Returns:** List of display handles connected to this device.

#### `get_dpaux`

```python
get_dpaux(self: dlsdk.Device, arg0: typing.SupportsInt | typing.SupportsIndex) -> dl::sdk::DpAux
```

Gets the DpAux associated with this device

#### `restart`

```python
restart(self: dlsdk.Device) -> dlsdk.dlsdk_status
```

Restarts the associated device

#### `update_firmware`

```python
update_firmware(self: dlsdk.Device, firmware_path: str = '') -> dlsdk.dlsdk_status
```

Updates the firmware of the device using the given firmware directory path.

## Display API

### `dlsdk.Display`

Methods for manipulating/controlling displays attached to a device.

#### `id`

```python
id(self: dlsdk.Display) -> str
```

String representation of the display identity

**Details:** Identifies a single output on a device; a device with multiple outputs (e.g. up to 4 HDMI connectors) exposes one identifier per output. Example: "USB_6015-10096418^0", "USB_6015-10096418^1", "USB_6015-10096418^2", "USB_6015-10096418^3", where 6015 is the USB product ID (in hex), 10096418 is the serial number, and the suffix after ^ is the output index. The DisplayLink USB vendor ID (0x17e9) is not included.

**Returns:** A string containing the device and head identifiers.

#### `size`

```python
size(self: dlsdk.Display) -> dlsdk.Rect
```

Get the current resolution of the display

#### `edid`

```python
edid(self: dlsdk.Display) -> tuple[dlsdk.dlsdk_status, list[int]]
```

Get the EDID data of the display

#### `modes`

```python
modes(self: dlsdk.Display) -> tuple[dlsdk.dlsdk_status, list[dlsdk.DisplayMode]]
```

Get the supported display modes

#### `preferred_mode`

```python
preferred_mode(self: dlsdk.Display) -> tuple[dlsdk.dlsdk_status, dlsdk.DisplayMode]
```

Get the preferred display mode

#### `get_capabilities`

```python
get_capabilities(self: dlsdk.Display) -> tuple[dlsdk.dlsdk_status, dlsdk.DisplayCapabilities]
```

Get the signal modes and pixel formats supported by the display

#### `power_on`

```python
power_on(self: dlsdk.Display, mode: dlsdk.DisplayMode | None = None, timing: dlsdk.DisplayTiming | None = None, signal_mode: dlsdk.dlsdk_display_signal_mode = dlsdk.DLSDK_DISPLAY_SIGNAL_MODE_SDR) -> dlsdk.dlsdk_status
```

Turn on the display, optionally using a specific mode or custom timing

**Parameters:**

- `mode` — Optional requested display mode.
- `timing` — Optional requested custom timing.
- `signal_mode` — Display signal mode. Defaults to `DLSDK_DISPLAY_SIGNAL_MODE_SDR`. `mode` and `timing` are mutually exclusive.

**Notes:** When using a `signal_mode` of `DLSDK_DISPLAY_SIGNAL_MODE_HDR10`, use `DLSDK_PIXEL_FORMAT_RGB10` or `DLSDK_PIXEL_FORMAT_XBGR16F` in `show()`.

#### `power_off`

```python
power_off(self: dlsdk.Display) -> dlsdk.dlsdk_status
```

Turn off the display

#### `show`

```python
show(self: dlsdk.Display, format: dlsdk.dlsdk_pixel_format, pixels: typing.Annotated[numpy.typing.ArrayLike, numpy.uint8], dirty_rects: collections.abc.Sequence[dlsdk.DirtyRect] = [], static_metadata: dlsdk.HdrStaticMetadata | None = None) -> dlsdk.dlsdk_status
```

Starts presenting pixel with given format to the display. When dirty_rects is given, it hints which regions changed since the previous frame so that only those may be updated. The rectangles must cover everything that changed, and pixels must still describe the entire display

**Parameters:**

- `format` — Pixel format contained in the pixel buffer.
- `pixels` — Pixel buffer describing the entire display.
- `dirty_rects` — Optional regions that changed since the previous frame.
- `static_metadata` — Optional static HDR metadata associated with the frame.

**Notes:** When the display is powered on in `DLSDK_DISPLAY_SIGNAL_MODE_HDR10` mode, use `DLSDK_PIXEL_FORMAT_RGB10` or `DLSDK_PIXEL_FORMAT_XBGR16F` for `format`.

#### `wait_on_show`

```python
wait_on_show(self: dlsdk.Display, timeout: datetime.timedelta = ...) -> dlsdk.dlsdk_status
```

Waits for pixels to be presented

**Notes:** `timeout` defaults to waiting indefinitely (mirroring the C++ `DisplayHandle::waitOnShow()` default of `std::chrono::milliseconds::max()`); pass an explicit `datetime.timedelta` to wait with a bounded timeout instead.

#### `clear`

```python
clear(self: dlsdk.Display) -> dlsdk.dlsdk_status
```

Clear the display

#### `getBrightness`

```python
getBrightness(self: dlsdk.Display) -> tuple[dlsdk.dlsdk_status, int]
```

Get the current brightness of the display

#### `setBrightness`

```python
setBrightness(self: dlsdk.Display, arg0: typing.SupportsInt | typing.SupportsIndex) -> dlsdk.dlsdk_status
```

Set the brightness of the display

#### `getDpAux`

```python
getDpAux(self: dlsdk.Display) -> dl::sdk::DpAux
```

Get the DP AUX channel associated with the display

## DP AUX API

Low-level access to the DisplayPort AUX channel, useful when direct AUX/I2C
access is required beyond the automatic display enumeration.

### `dlsdk.DpAux`

Methods for the DP AUX channel.

#### `read`

```python
read(self: dlsdk.DpAux, aux_reg_address: typing.SupportsInt | typing.SupportsIndex, data: collections.abc.Buffer) -> dlsdk.dlsdk_aux_status
```

Read data from DP AUX channel

**Notes:** The underlying API uses 16-byte burst transactions. Larger requests are serviced by
chaining multiple burst reads into the provided buffer.


#### `write`

```python
write(self: dlsdk.DpAux, aux_reg_address: typing.SupportsInt | typing.SupportsIndex, data: collections.abc.Buffer) -> dlsdk.dlsdk_aux_status
```

Write data to DP AUX channel

**Notes:** The underlying API uses 16-byte burst transactions. Larger requests are split into
multiple burst writes from the provided buffer.

#### `i2c_read`

```python
i2c_read(self: dlsdk.DpAux, device_address: typing.SupportsInt | typing.SupportsIndex, data: collections.abc.Buffer, mot: bool, timeout: datetime.timedelta = datetime.timedelta(seconds=1)) -> dlsdk.dlsdk_aux_status
```

Read I2C data by an AUX transaction

#### `i2c_write`

```python
i2c_write(self: dlsdk.DpAux, device_address: typing.SupportsInt | typing.SupportsIndex, data: collections.abc.Buffer, mot: bool, timeout: datetime.timedelta) -> dlsdk.dlsdk_aux_status
```

Write I2C data by an AUX transaction

#### `i2c_write_and_read`

```python
i2c_write_and_read(self: dlsdk.DpAux, device_address: typing.SupportsInt | typing.SupportsIndex, write_data: collections.abc.Buffer, read_data: collections.abc.Buffer, timeout: datetime.timedelta) -> dlsdk.dlsdk_aux_status
```

Write and read I2C data by an AUX transaction

#### `begin_exclusive_access`

```python
begin_exclusive_access(self: dlsdk.DpAux, arg0: datetime.timedelta) -> dlsdk.dlsdk_status
```

Begin exclusive access to the DP AUX channel

#### `end_exclusive_access`

```python
end_exclusive_access(self: dlsdk.DpAux) -> dlsdk.dlsdk_status
```

End exclusive access to the DP AUX channel

#### `detect`

```python
detect(self: dlsdk.DpAux) -> dlsdk.dlsdk_status
```

Detects whether a monitor is connected to the DP AUX channel and forces link
training.

#### `enable_virtual_hpd_polling`

```python
enable_virtual_hpd_polling(self: dlsdk.DpAux) -> dlsdk.dlsdk_status
```

Enable virtual HPD polling for the DP AUX channel

#### `disable_virtual_hpd_polling`

```python
disable_virtual_hpd_polling(self: dlsdk.DpAux) -> dlsdk.dlsdk_status
```

Disable virtual HPD polling for the DP AUX channel
