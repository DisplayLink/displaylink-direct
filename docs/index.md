# DisplayLink Direct

**DisplayLink Direct** is a software development kit (SDK) that lets applications
render content directly to displays connected through DisplayLink&reg; hardware.
DisplayLink (developed by Synaptics) is a software-aware hardware protocol that
encodes and transmits video over standard data buses, such as USB 2.0 and USB
3.2 Gen 1, without relying on native video pass-through such as DisplayPort Alt Mode
or Thunderbolt.

Instead of pushing raw, uncompressed pixels down a native display pipeline,
DisplayLink compresses frame deltas on the host and packetizes that data over USB
bulk transfers to a hardware decoder chip inside the dock or dongle, which drives
the attached displays.

## Motivation

Unlike a traditional DisplayLink deployment, DisplayLink Direct **does not require
host-side graphics drivers or compositor integration**. Your application talks to
the SDK through a small, C-compatible API and submits frame buffers directly to
each display. This makes it a lightweight, self-contained solution that is
well-suited to embedded systems, kiosks, digital signage, and any application
that needs deterministic, driver-free multi-display output.
