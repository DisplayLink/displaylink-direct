# Supported Platforms

This release of DisplayLink Direct supports the following host platforms.

## Operating systems and architectures

| Operating system | Architectures        | Status    |
| ---------------- | -------------------- | --------- |
| Linux            | `x86_64`, `aarch64`  | Supported |
| Android            | `aarch64`  | Supported |

!!! info "More platforms coming"
    Windows, macOS, and Linux `riscv64` are planned for future releases and are
    not yet supported.

## Host connection

- A **USB port** is required to communicate with DisplayLink devices.
- **Usb 3.2 Gen 1 or later** is recommended for full display bandwidth.

## Programming languages

- **C / C++** — the primary API. Client software interfaces with the
  C-compatible API calls in the SDK.
- **Python** — an optional wrapper for Python client code, requiring
  **Python 3.10 or higher**.

