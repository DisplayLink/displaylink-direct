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
