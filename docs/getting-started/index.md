# Getting Started

Everything you need to install, configure, and run DisplayLink Direct is
published as [GitHub Release Assets]({{ config.repo_url }}/releases/).

## Getting the package

Download all necessary components (source code, core libraries, and firmware)
from the [latest release]({{ config.repo_url }}/releases/latest)
or a [specific release tag]({{ config.repo_url }}/releases).

The repository provides:

- **Source code**: C/C++ headers ([`include/`]({{ config.repo_url }}/tree/{{ main_branch }}/include)), supporting files (license, third-party licenses, CMake config files), release notes, and package metadata.
- **Core libraries**: Proprietary pre-built binaries distributed via GitHub Release Assets (download into [`libs/`]({{ config.repo_url }}/tree/{{ main_branch }}/libs)).
- **Firmware**: DisplayLink device firmware packages distributed via GitHub Release Assets (download into [`firmware/`]({{ config.repo_url }}/tree/{{ main_branch }}/firmware)). See [Firmware Update](firmware-update.md) for the update process.

=== "Linux"

    ```bash
    # Download source code archive
    curl -sL {{ config.repo_url }}/archive/refs/tags/v{{ latest_version }}.tar.gz -o displaylink-direct.tar.gz
    tar -xzf displaylink-direct.tar.gz

    # Download and extract core libraries
    curl -sL {{ config.repo_url }}/releases/download/v{{ latest_version }}/binaries.tar.gz -o binaries.tar.gz
    tar -xzf binaries.tar.gz -C displaylink-direct-{{ latest_version }}/libs/

    # Download and extract firmware
    curl -sL {{ config.repo_url }}/releases/download/v{{ latest_version }}/firmware.tar.gz -o firmware.tar.gz
    tar -xzf firmware.tar.gz -C displaylink-direct-{{ latest_version }}/firmware/
    ```

## In this section

- [Running on Linux](installation-linux.md)
- [Running on Android](installation-android.md)
- [Firmware Update](firmware-update.md)
- [Python Bindings](python-bindings.md)
