# Minimal Usage Example

This example powers on the first display of the first attached device and fills
it with brand blue (`#0081c6`). It assumes that exactly one device and one
display are connected.

Repository source files for these examples:

- C: [samples/minimal-c/minimal-example.c]({{ config.repo_url }}/blob/{{ main_branch }}/samples/minimal-c/minimal-example.c)
- C++: [samples/minimal-c++/minimal-example.cpp]({{ config.repo_url }}/blob/{{ main_branch }}/samples/minimal-c++/minimal-example.cpp)
- Python: [samples/minimal-python/minimal-example.py]({{ config.repo_url }}/blob/{{ main_branch }}/samples/minimal-python/minimal-example.py)

=== "C"

    ```c
    #include <dlsdk/dlsdk.h>
    #include <assert.h>
    #include <stdint.h>
    #include <stdlib.h>
    #ifdef _WIN32
    #include <windows.h>
    #else
    #include <unistd.h>
    #endif

    int main(void)
    {
      dlsdk_initialise();

      dlsdk_device_handle device;
      unsigned int device_count = 1;
      assert(dlsdk_get_devices(&device, &device_count) == DLSDK_SUCCESS && device_count == 1); /* exactly one device connected */

      dlsdk_display_handle display;
      unsigned int display_count = 1;
      assert(dlsdk_device_get_displays(device, &display, &display_count) == DLSDK_SUCCESS && display_count == 1); /* exactly one display connected */

      dlsdk_display_power_on(display);

      struct dlsdk_rect size = dlsdk_display_size(display);
      uint32_t* pixels = (uint32_t*)malloc((size_t)size.width * size.height * sizeof(uint32_t));
      for (unsigned int i = 0; i < size.width * size.height; ++i) {
        pixels[i] = 0xFF0081C6; /* brand blue #0081c6, DLSDK_PIXEL_FORMAT_XRGB byte order is B,G,R,X */
      }

      dlsdk_frame frame;
      DLSDK_FRAME_INIT(&frame, DLSDK_PIXEL_FORMAT_XRGB, pixels, size.width * size.height * sizeof(uint32_t));
      dlsdk_display_show(display, &frame);
      dlsdk_display_wait_on_show(display);

      /* Keep the frame visible for 10 seconds. */
      #ifdef _WIN32
      Sleep(10000);
      #else
      sleep(10);
      #endif

      free(pixels);
      dlsdk_teardown();
      return 0;
    }
    ```

=== "C++"

    ```cpp
    #include <dlsdk/dlsdk.h>
    #include <cassert>
    #include <chrono>
    #include <cstdint>
    #include <thread>
    #include <vector>

    int main()
    {
      dl::sdk::System system;

      std::vector<dl::sdk::DeviceHandle> devices = system.getDevices();
      assert(devices.size() == 1); // exactly one device connected
      dl::sdk::DeviceHandle& device = devices.front();

      std::vector<dl::sdk::DisplayHandle> displays = device.getDisplays();
      assert(displays.size() == 1); // exactly one display connected
      dl::sdk::DisplayHandle& display = displays.front();

      display.powerOn();

      dl::sdk::Rect size = display.size();
      std::vector<uint32_t> pixels(size.width * size.height, 0xFF0081C6); // brand blue #0081c6, DLSDK_PIXEL_FORMAT_XRGB byte order is B,G,R,X

      dlsdk_frame frame;
      DLSDK_FRAME_INIT(&frame, DLSDK_PIXEL_FORMAT_XRGB, pixels.data(), pixels.size() * sizeof(uint32_t));
      display.show(frame);
      display.waitOnShow();

      std::this_thread::sleep_for(std::chrono::seconds(10)); // keep the frame visible for 10 seconds

      return 0;
    }
    ```

=== "Python"

    ```python
    import time

    import numpy as np
    import dlsdk

    dlsdk.create_system()

    devices = dlsdk.get_devices()
    assert len(devices) == 1  # exactly one device connected
    device = devices[0]

    displays = device.get_displays()
    assert len(displays) == 1  # exactly one display connected
    display = displays[0]
    display.power_on()

    size = display.size()
    frame = np.empty((size.height, size.width, 4), dtype=np.uint8)
    frame[:, :, 0] = 0xC6  # B
    frame[:, :, 1] = 0x81  # G
    frame[:, :, 2] = 0x00  # R
    frame[:, :, 3] = 0xFF  # X (unused), DLSDK_PIXEL_FORMAT_XRGB byte order is B,G,R,X

    display.show(dlsdk.DLSDK_PIXEL_FORMAT_XRGB, frame)
    display.wait_on_show()

    time.sleep(10)  # keep the frame visible for 10 seconds

    dlsdk.delete_system()
    ```
