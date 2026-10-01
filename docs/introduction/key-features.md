# Key Features

DisplayLink Direct provides a compact, C-compatible API for driving DisplayLink
displays without host-side drivers. Its main capabilities include:

## Driver-free, lightweight integration

- **No host graphics drivers or compositor integration required.** The SDK
  communicates with DisplayLink hardware directly over USB.
- **No host GPU required.** Frames are encoded on the CPU and decoded by the
  DisplayLink device, so the SDK runs on headless servers, GPU-less
  single-board computers, and minimal embedded systems.
- **Embedded mode** for resource-constrained systems: when enabled, the SDK does
  not allocate dynamic memory at runtime, making its footprint predictable and
  suitable for embedded deployments.

## Multi-display output

- Enumerate connected DisplayLink **devices** and their **displays** through a
  simple system → device → display hierarchy.
- Drive **multiple displays** from a single host over one USB connection, with
  helpers to synchronize output across displays.
- Stable **hardware identifiers** for devices and displays (based on the USB
  serial number), so the correct image is always routed to the correct output —
  even after the device is unplugged and reconnected or power-cycled.

## Flexible frame submission

- Submit frame buffers directly to a display and present them with a
  show/wait-on-show model.
- Support for a range of **pixel formats**, including 32-bit RGB variants
  (`XRGB`, `XBGR`, `ARGB`, `ABGR`), packed 24-bit, and additional YUV and
  high-bit-depth formats.
- Built-in frame-delta compression handled by the SDK and DisplayLink hardware.

## Display management

- Power displays on and off and query display size.
- Read a display's **EDID** to discover its capabilities.
- Access the DisplayPort AUX channel for advanced use cases.

## Language bindings and integrations

- Native **C/C++** API.
- Optional **Python** bindings (Python 3.10 or higher).
- A **Qt platform plugin** for rendering Qt applications to DisplayLink displays.
- Ready-to-run [samples](../code-samples/index.md), including a video wall and an
  EDID reader.

