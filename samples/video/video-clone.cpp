#include <dlsdk/dlsdk.h>
#include <atomic>
#include <csignal>
#include "common.hpp"

#ifndef USE_GSTREAMER
#include "common_vlc.hpp"
#else
#include "common_gst.hpp"
#endif

std::atomic_bool terminate_program(false);

static std::vector<dl::sdk::DeviceHandle> g_devices;
static std::vector<dl::sdk::DisplayHandle> g_displays;

static void OnDisplay(void* object, void* picture)
{
  (void)(object);
  const unsigned displayRowInBytes = g_displays.at(0).size().width * 4;
  dlsdk_frame frame;
  DLSDK_FRAME_INIT(&frame, DLSDK_PIXEL_FORMAT_XRGB, static_cast<uint8_t *>(picture), displayRowInBytes * g_displays.at(0).size().height);
  for (auto& disp : g_displays) {
    disp.show(frame);
  }
  for (auto& disp : g_displays) {
    disp.waitOnShow();
  }
}

static void EnumerateDisplays()
{
  for (auto& dev : g_devices) {
    auto displays = dev.getDisplays();
    std::cout << "Device: " << dev.id() << " has " << displays.size() << " outputs" << std::endl;
    for (auto& disp : displays) {
      disp.powerOn();
      g_displays.emplace_back(std::move(disp));
    }
  }

  std::cout << "Found " << g_displays.size() << " connected monitors" << std::endl;
}

static auto GetResolution()
{
  // NOTE: This works assuming all the monitors have the same resolution

  if (g_displays.size() == 0) {
    ExitFail("No monitors available");
  }

  const auto size = g_displays.at(0).size();
  for (const auto& d : g_displays) {
    if (d.size().width != size.width || d.size().height != size.height) {
      ExitFail("This app only supports monitor with the same resolution");
    }
  }

  return size;
}

void exit_handler(int signal) {

  std::cout << "exit_handler called with signal " << signal << std::endl;

  terminate_program=1;
}

int main(int argc, const char* argv[])
{
  std::signal(SIGTERM, exit_handler);
  std::signal(SIGINT, exit_handler);

  dl::sdk::System system;

  g_devices = system.getDevices();
  std::cout << "Found " << g_devices.size() << " devices" << std::endl;

  dl::sdk::CallbackHandle callbackHandle;
  auto callback = [](const dl::sdk::HotplugEventData& event) {
    std::cout << "hotplug event=" << static_cast<int>(event.event)
    << " device=" << event.device_id
    << " display=" << event.display_id
    << std::endl;
    if (event.event == DLSDK_HOTPLUG_EVENT_DEVICE_REMOVED
        || event.event == DLSDK_HOTPLUG_EVENT_DISPLAY_REMOVED
    ) {
      std::cout << "Device or display removed, ending program." << std::endl;
      terminate_program = true;
    }
  };

  system.registerHotplugCallback(callback, &callbackHandle);

  if (argc < 2) {
      std::cout << "To play a video:  video-clone <image_file> [rotate]";
      return g_devices.size();
  }

  const auto filename = argv[1];
  const bool rotate = ((argc > 2) && std::string{ argv[2] } == "rotate") ? true : false;
  int timeout = (argc > 3) ? atoi(argv[3]) : 60;

  EnumerateDisplays();
  const auto size = GetResolution();
  std::cout << "Resolution " << size.width << " x " << size.height << std::endl;

  std::vector<uint8_t> frameBuffer;
  constexpr unsigned BytesPerPixel = 4;
  frameBuffer.resize(size.width * size.height * BytesPerPixel);

  auto* player = RunPlayer(filename, frameBuffer.data(), size.width, size.height, BytesPerPixel, rotate, OnDisplay);

  std::cout << "Program will terminate in " << timeout <<  " seconds."
    "\nYou can also press ctrl+c to terminate the program early" << std::endl;

  while (!terminate_program && timeout > 0) {
    timeout--;
    SleepSeconds(1);
  }

  StopPlayer(player);

  g_displays.clear();
  g_devices.clear();

  system.unregisterHotplugCallback(callbackHandle);
  dlsdk_teardown();

  return 0;
}
