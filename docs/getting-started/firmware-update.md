# Firmware Update

For proper operation, the DisplayLink device firmware must be kept up to date.
DisplayLink Direct is validated against the firmware versions listed in the
release notes for each release.

!!! note "Embedded mode"
    Firmware updates are **not available in embedded mode**.
    Ensure the device is running compatible firmware before using embedded mode.

    To update firmware, perform the update in non-embedded mode first.

## Firmware packages

Firmware is distributed as pre-built binary packages (`.spkg` files) via GitHub
Release Assets. Download the firmware package `firmware.zip` from the
[release assets]({{ config.repo_url }}/releases/) and
unpack it into the
[`firmware/`]({{ config.repo_url }}/tree/{{ main_branch }}/firmware)
directory of your checkout.

Each `.spkg` file targets a specific DisplayLink device family, for example:

- `ella-dock-release.spkg` for DL-3xxx
- `firefly-monitor-release.spkg` for DL-4xxx
- `ridge-dock-release.spkg` for DL-6xxx
- `navarro-dock-release.spkg` for DL-7xxx

## Supported firmware versions

The firmware versions bundled with the current release are published in the
release notes.

!!! note "Firmware compatibility"
    Older DisplayLink Direct releases may be incompatible with devices running
    newer firmware. In that case, the device may not be enumerated by the SDK
    at all.

    When a device is running outdated firmware, it may still enumerate, but
    some capabilities may be unavailable or may behave differently until the
    firmware is updated.

### Read firmware version

Use the device handle to query the firmware version currently running on the
device.

=== "C"

    ```c
    #include <stdio.h>
    #include <dlsdk/dlsdk.h>

    dlsdk_initialise();

    dlsdk_device_handle devices[4];
    unsigned int count = 4;
    if (dlsdk_get_devices(devices, &count) == DLSDK_SUCCESS && count > 0) {
        for (unsigned int i = 0; i < count; ++i) {
            const char* version = dlsdk_device_firmware_version(devices[i]);
            printf("Device %u firmware version: %s\n", i, version ? version : "unknown");
        }
    }

    dlsdk_teardown();
    ```

=== "C++"

    ```cpp
    #include <iostream>
    #include <dlsdk/dlsdk.h>

    dl::sdk::System system;
    auto devices = system.getDevices();

    for (std::size_t i = 0; i < devices.size(); ++i) {
        std::cout << "Device " << i
                  << " firmware version: " << devices[i].firmwareVersion()
                  << std::endl;
    }
    ```

=== "Python"

    ```python
    import dlsdk

    dlsdk.create_system(dlsdk.Config())

    devices = dlsdk.get_devices()
    for index, device in enumerate(devices):
        print(f"Device {index} firmware version: {device.firmware_version()}")

    dlsdk.delete_system()
    ```


## Update process

Firmware is applied by the SDK when a connected device is running a different
version than the provided firmware. Ensure the `firmware/` directory contains
the correct `.spkg` packages before initialising the system.

!!! warning "Firmware path must point to a directory"
    All firmware update API calls require a path to a directory containing the
    `.spkg` files, not a path to a single firmware file. During the update
    process, DisplayLink Direct identifies the enumerated device family and selects the
    matching firmware package from that directory.

There are two ways to update firmware: **automatic** update at initialization,
or **manual** update on demand.


### Automatic firmware update

When the system is initialised with a valid firmware path, the SDK
automatically updates any connected device whose firmware differs from the
packages in that directory. This is the recommended approach for most
applications.

!!! warning "Wait for the update to finish"
    Do not disconnect the device or tear down the system while a firmware
    update is in progress. Interrupting the update may cause the device to
    boot from its backup firmware, and may disable fast firmware updates on
    the next boot.



=== "C"

    ```c
    #include <time.h>
    #include <dlsdk/dlsdk.h>

    dlsdk_config config;
    DLSDK_CONFIG_INIT(&config);
    config.firmwarePath = "./firmware"; // enables automatic update

    dlsdk_initialise_with_config(&config);

    // Wait up to 30 seconds for the device to appear after the automatic
    // firmware update completes and the device re-enumerates.
    time_t deadline = time(NULL) + 30;
    unsigned int count = 0;
    while (time(NULL) < deadline) {
        dlsdk_get_devices(NULL, &count);
        if (count > 0) break;
        struct timespec ts = {0, 500000000L}; // 500 ms
        nanosleep(&ts, NULL);
    }

    dlsdk_teardown();
    ```

=== "C++"

    ```cpp
    #include <chrono>
    #include <thread>
    #include <dlsdk/dlsdk.h>

    // Passing a firmware path enables automatic update.
    dl::sdk::Config config("./firmware");
    dl::sdk::System system(config);

    // Wait up to 30 seconds for the device to appear after the automatic
    // firmware update completes and the device re-enumerates.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (system.getDevices().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    ```

=== "Python"

    ```python
    import time
    import dlsdk

    # Config() uses the firmware bundled with the package by default.
    config = dlsdk.Config()
    dlsdk.create_system(config)

    # Wait up to 30 seconds for the device to appear after the automatic
    # firmware update completes and the device re-enumerates.
    timeout = time.monotonic() + 30
    while not dlsdk.get_devices() and time.monotonic() < timeout:
        time.sleep(0.5)

    dlsdk.delete_system()
    ```

### Manual firmware update

To control when the update happens, disable automatic update by initialising
the system with an empty or `None` firmware path, then call the update function
explicitly for the device.

!!! note "Handles after update"
    After a successful update, the device and display handles remain valid,
    but the display must be powered on again.

=== "C"

    ```c
    #include <dlsdk/dlsdk.h>

    dlsdk_config config;
    DLSDK_CONFIG_INIT(&config);

    dlsdk_initialise_with_config(&config);

    dlsdk_device_handle devices[4];
    unsigned int count = 4;
    if (dlsdk_get_devices(devices, &count) == DLSDK_SUCCESS) {
      for (unsigned int i = 0; i < count; ++i) {
        // Blocks until the update completes (up to ~30 seconds).
        dlsdk_device_update_firmware(devices[i], "./firmware");
      }
    }

    dlsdk_teardown();
    ```

=== "C++"

    ```cpp
    #include <dlsdk/dlsdk.h>

    // Passing nullptr disables automatic update.
    dl::sdk::Config config(nullptr);
    dl::sdk::System system(config);

    for (auto& device : system.getDevices()) {
      // Blocks until the update completes (up to ~30 seconds).
      device.updateFirmware("./firmware");
    }
    ```

=== "Python"

    ```python
    import dlsdk

    # Keep the bundled firmware path, but disable automatic update.
    config = dlsdk.Config()
    firmware_path = config.firmware_path
    config.firmware_path = None

    dlsdk.create_system(config)

    for device in dlsdk.get_devices():
        # Blocks until the update completes.
        dlsdk.update_firmware(device, str(firmware_path))

    dlsdk.delete_system()
    ```
