# Running on Linux

## Supported architectures and distributions

DisplayLink Direct supports the following Linux platforms:

- **x86_64** (Intel/AMD 64-bit)
- **aarch64** (ARM 64-bit)

The SDK is compatible with most modern Linux distributions. Ensure your system
meets the minimum C runtime and library requirements before installation.

## Runtime dependencies

The following system libraries are required to run DisplayLink Direct:

- **libusb >= 1.0.16** — for USB device communication
- **libc6 >= 2.31** — C runtime library

These libraries are typically pre-installed on most Linux distributions. If your
system is missing them, install them using your distribution's package manager:

=== "Debian/Ubuntu"

    ```bash
    sudo apt-get update
    sudo apt-get install libusb-1.0-0 libc6
    ```

=== "RHEL/CentOS/Fedora"

    ```bash
    sudo yum install libusb libc
    ```

=== "Alpine"

    ```bash
    apk add libusb libc6
    ```


## Download the library

Download the pre-built library package for your architecture from the GitHub
[Release Assets]({{ config.repo_url }}/releases).


## Building with CMake

To integrate the SDK into your CMake project, use `find_package` to locate the
SDK and link against the `dlsdk::dlsdk` target:

```cmake
find_package(dlsdk {{ latest_version }} REQUIRED)

add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE dlsdk::dlsdk)
```

Then configure and build, passing the SDK installation path via
`CMAKE_PREFIX_PATH`:

```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/dlsdk -S . -B build
cmake --build build
```

Replace `/path/to/dlsdk` with the path to your DisplayLink Direct
installation.

## Running applications

### Disable the standard DisplayLink driver

If the standard DisplayLink driver (`displaylink-driver`) is installed on your
system, it must be stopped and masked before using DisplayLink Direct because
the driver and DisplayLink Direct cannot both manage the device simultaneously:

```bash
sudo systemctl stop displaylink-driver
sudo systemctl mask displaylink-driver
```

### USB permissions

By default, access to DisplayLink USB devices requires root privileges. To run
applications without root, set up udev rules to grant access to users in the
`displaylink` group:

```bash
sudo groupadd displaylink
sudo usermod -aG displaylink $USER

echo 'SUBSYSTEM=="usb", MODE="0660", GROUP="displaylink"' | sudo tee /etc/udev/rules.d/00-usb-permissions.rules
sudo udevadm control --reload-rules

```

Log out and log back in for group membership changes to take effect. Then you
can run applications without the `sudo` prefix.
