# Embedded Mode

DisplayLink Direct can run in two modes, selected at initialization time through
the SDK's configuration. The mode controls how the SDK manages memory at runtime,
letting you trade flexibility for determinism depending on your target system.

!!! warning "Hardware support required"
    Embedded mode is supported only on selected DisplayLink devices based on the
    **DL-7xxx** family chipset. It is not available on other devices. For details
    on compatible hardware and enabling this feature, please
    **[contact Synaptics](https://www.synaptics.com/company/contact-us)**.

## Non-embedded mode (default)

In the default mode, the SDK allocates memory dynamically at runtime as needed.
This is the most flexible option and is appropriate for general-purpose hosts
such as desktops, laptops, and servers, where dynamic allocation is not a concern.

- Simplest to use — no special hardware requirements.
- Suitable for the majority of applications.
- Memory usage adapts to the current workload.
- Higher performance at high resolutions and refresh rates.

## Embedded mode

In embedded mode, **the SDK does not use dynamic memory at runtime**. This gives
predictable, deterministic resource usage, which is important for embedded and
resource-constrained systems, long-running deployments, and environments where
runtime allocation is undesirable.

- No runtime dynamic memory allocation.
- Predictable, deterministic memory footprint.
- Well-suited to embedded, industrial, and safety-conscious deployments.
- May be constrained at high resolutions and refresh rates compared to
  non-embedded mode.

!!! note "Firmware update not available"
    Automatic and manual firmware updates are **not available in embedded mode**.
    Ensure the device is running compatible firmware before using embedded mode.

### Resolution limits

In embedded mode, the maximum supported resolution depends on how many outputs
are driven:

| Configuration      | Max width | Max height |
| ------------------ | --------- | ---------- |
| Single output      | 5120 px   | 2160 px    |
| Multiple outputs   | 1920 px   | 1080 px    |

## Feature Comparison

| Consideration            | Non-embedded (default) | Embedded            |
| ------------------------ | ---------------------- | ------------------- |
| Runtime memory allocation | Dynamic               | None                |
| Resource predictability   | Adapts to workload    | Deterministic       |
| High display modes        | Higher performance    | May be constrained  |
| Hardware requirement      | Any supported device  | DL-7xxx family only |
| Typical hosts             | Desktop, laptop, server | Embedded, industrial |

## Enabling embedded mode

Embedded mode is enabled through the SDK configuration when initialising the
system. Set the `DLSDK_CONFIG_FLAG_EMBEDDED_MODE` flag in the configuration
before initialisation:

=== "C"

    ```c
    #include <dlsdk/dlsdk.h>

    dlsdk_config config;
    DLSDK_CONFIG_INIT(&config);
    config.flags = DLSDK_CONFIG_FLAG_EMBEDDED_MODE;

    dlsdk_initialise_with_config(&config);
    ```

=== "C++"

    ```cpp
    #include <dlsdk/dlsdk.h>

    // firmwarePath = nullptr, firmware update not supported in embedded mode
    dl::sdk::Config config(nullptr, /*flags=*/DLSDK_CONFIG_FLAG_EMBEDDED_MODE);
    dl::sdk::System system(config);
    ```

=== "Python"

    ```python
    import dlsdk

    config = dlsdk.Config()
    config.embedded_mode = True
    dlsdk.create_system(config)
    ```

!!! note
    If the connected device does not support embedded mode, enabling this flag
    will not have the intended effect. Ensure you are using a compatible DL-7xxx
    family device — [contact Synaptics](https://www.synaptics.com/company/contact-us)
    if you are unsure.

