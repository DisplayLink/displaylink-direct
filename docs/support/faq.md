# FAQ

This page answers common questions when setting up and running DisplayLink
Direct.

## Why does dlsdk_get_devices return no devices?

If dlsdk_get_devices returns no devices, work through these checks:

- Make sure USB permissions are configured for your user account. See [USB
	permissions](../getting-started/installation-linux.md#usb-permissions) in the
	Linux getting started guide.
- The device may not be enumerated yet if you call dlsdk_get_devices
  immediately after dlsdk_initialise, or while an automatic firmware update is
  still in progress. Use the [hotplug
  API](../api-reference/api-documentation.md#dlsdk_hotplug_event), or call
  dlsdk_get_devices in a loop until the device is detected.
- Stop and uninstall the standard DisplayLink driver. The standard driver and
	DisplayLink Direct cannot control the same device at the same time. See
	[Disable the standard DisplayLink
	driver](../getting-started/installation-linux.md#disable-the-standard-displaylink-driver).
- Make sure no other process is currently using DisplayLink Direct or holding a
	device handle.

## Which firmware package should I use?

When you call dlsdk_device_update_firmware, pass a directory path that contains
the .spkg firmware files.

DisplayLink Direct will automatically select the firmware package that matches
the connected device.
