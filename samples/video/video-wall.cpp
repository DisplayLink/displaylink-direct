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

static constexpr unsigned BytesPerPixel = 4;
static unsigned g_stride;
static unsigned g_displayOffset;
static std::vector<dl::sdk::DeviceHandle> g_devices;
static std::vector<dl::sdk::DisplayHandle> g_displays;
static dl::sdk::Rect g_size;

static void OnDisplayQuad(void* object, void* picture)
{
  (void)(object);
  auto input = static_cast<uint8_t *>(picture);
  const unsigned displayCount = g_displays.size();
  const unsigned displayRowInBytes = g_size.width * BytesPerPixel;

  dlsdk_frame frame1;
  DLSDK_FRAME_INIT(&frame1, DLSDK_PIXEL_FORMAT_XRGB, input, displayRowInBytes * g_size.height);
  frame1.stride = g_stride;
  dlsdk_frame frame2;
  DLSDK_FRAME_INIT(&frame2, DLSDK_PIXEL_FORMAT_XRGB, input + displayRowInBytes, displayRowInBytes * g_size.height);
  frame2.stride = g_stride;
  dlsdk_frame frame3;
  DLSDK_FRAME_INIT(&frame3, DLSDK_PIXEL_FORMAT_XRGB, input + g_displayOffset, displayRowInBytes * g_size.height);
  frame3.stride = g_stride;
  dlsdk_frame frame4;
  DLSDK_FRAME_INIT(&frame4, DLSDK_PIXEL_FORMAT_XRGB, input + displayRowInBytes + g_displayOffset, displayRowInBytes * g_size.height);
  frame4.stride = g_stride;

  g_displays.at(0).show(frame1);
  g_displays.at(1).show(frame2);
  g_displays.at(2).show(frame3);
  g_displays.at(3).show(frame4);

  for (auto &disp : g_displays) {
    disp.waitOnShow();
  }
}

static void OnDisplayRow(void* object, void* picture)
{
  (void)(object);
  const auto input = static_cast<uint8_t *>(picture);
  const unsigned displayCount = g_displays.size();
  const unsigned displayRowInBytes = g_size.width * BytesPerPixel;

  for (unsigned i = 0; i < displayCount; ++i) {
    auto &disp = g_displays.at(i);
    dlsdk_frame frame;
    DLSDK_FRAME_INIT(&frame, DLSDK_PIXEL_FORMAT_XRGB, input + i * g_displayOffset, displayRowInBytes * g_size.height);
    frame.stride = g_stride;
    disp.show(frame);
  }

  for (auto &disp : g_displays) {
    disp.waitOnShow();
  }
}

static dl::sdk::Rect CalculateComposite()
{
  const unsigned displayCount = g_displays.size();
  dl::sdk::Rect composite;
  g_size = g_displays.at(0).size();
  if (displayCount == 4) {
    composite = dl::sdk::Rect{g_size.width * 2, g_size.height * 2};
    g_stride = composite.width * BytesPerPixel;
    g_displayOffset = composite.width * g_size.height * BytesPerPixel;
  } else {
    composite = dl::sdk::Rect{g_size.width, g_size.height * displayCount};
    g_stride = composite.width * BytesPerPixel;
    g_displayOffset = g_size.width * g_size.height * BytesPerPixel;
  }
  return composite;
}

static void EnumerateDisplays()
{
  for (auto &dev : g_devices) {
    auto displays = dev.getDisplays();
    g_size = displays.at(0).size();
    std::cout << "Device: " << dev.id() << " has " << displays.size() << " outputs" << std::endl;
    for (auto &disp : displays) {
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

  const unsigned displayCount = g_displays.size();
  auto composite = CalculateComposite();
  const dl::sdk::Rect size = g_displays.at(0).size();
  for (unsigned i = 1; i < displayCount; ++i) {
    const auto &d = g_displays.at(i);
    if (d.size().width != size.width || d.size().height != size.height) {
      ExitFail("This app only supports monitor with the same resolution");
    }
  }
  return composite;
}

void exit_handler(int signal) {

  std::cout << "exit_handler called with signal " << signal << std::endl;

  terminate_program=1;
}

int main(int argc, const char *argv[])
{
  std::signal(SIGTERM, exit_handler);
  std::signal(SIGINT, exit_handler);

  dl::sdk::System system;
  dl::sdk::CallbackHandle callbackHandle;

  g_devices = system.getDevices();
  std::cout << "Found " << g_devices.size() << " devices" << std::endl;

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
      std::cout << "To play a video:  video-wall <image_file> [rotate]";
      return g_devices.size();
  }

  const auto filename = argv[1];
  const bool rotate = ((argc > 2) && std::string{ argv[2] } == "rotate") ? true : false;
  int timeout = (argc > 3) ? atoi(argv[3]) : 60;

  EnumerateDisplays();
  const auto size = GetResolution();
  std::cout << "Resolution " << size.width << " x " << size.height << std::endl;

  std::vector<uint8_t> frameBuffer;
  frameBuffer.resize(size.width * size.height * BytesPerPixel);

  auto onDisplay = (g_displays.size() == 4) ? OnDisplayQuad : OnDisplayRow;
  auto* player = RunPlayer(filename, frameBuffer.data(), size.width, size.height, BytesPerPixel, rotate, onDisplay);

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
