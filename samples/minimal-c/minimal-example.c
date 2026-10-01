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
