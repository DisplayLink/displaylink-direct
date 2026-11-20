// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#include "dlsdk_session.h"

#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "log.h"

static std::string redirectPath(const char* path) {
  if (!path) return "";
  std::string p = path;
  if (p.rfind("/data/local/tmp/", 0) == 0) {
    const char* dir = getenv("HOME");
    if (!dir) dir = getenv("TMPDIR");
    if (dir) {
      std::string redirected = std::string(dir) + "/" + p.substr(16);
      LOGI("Redirecting file access: %s -> %s", path, redirected.c_str());
      return redirected;
    }
  }
  return p;
}

extern "C" {
__attribute__((visibility("default")))
FILE* fopen(const char* path, const char* mode) {
  std::string redirected = redirectPath(path);
  using FopenFn = FILE* (*)(const char*, const char*);
  static FopenFn real_fopen = reinterpret_cast<FopenFn>(dlsym(RTLD_NEXT, "fopen"));
  return real_fopen ? real_fopen(redirected.c_str(), mode) : nullptr;
}

__attribute__((visibility("default")))
int remove(const char* path) {
  std::string redirected = redirectPath(path);
  using RemoveFn = int (*)(const char*);
  static RemoveFn real_remove = reinterpret_cast<RemoveFn>(dlsym(RTLD_NEXT, "remove"));
  return real_remove ? real_remove(redirected.c_str()) : -1;
}

__attribute__((visibility("default")))
int access(const char* path, int mode) {
  std::string redirected = redirectPath(path);
  using AccessFn = int (*)(const char*, int);
  static AccessFn real_access = reinterpret_cast<AccessFn>(dlsym(RTLD_NEXT, "access"));
  return real_access ? real_access(redirected.c_str(), mode) : -1;
}

__attribute__((visibility("default")))
int open(const char* path, int flags, ...) {
  std::string redirected = redirectPath(path);
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = static_cast<mode_t>(va_arg(args, int));
    va_end(args);
  }
  using OpenFn = int (*)(const char*, int, mode_t);
  static OpenFn real_open = reinterpret_cast<OpenFn>(dlsym(RTLD_NEXT, "open"));
  return real_open ? real_open(redirected.c_str(), flags, mode) : -1;
}

__attribute__((visibility("default")))
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  // If attempting to mmap a character device (such as /dev/bus/usb/...), avoid triggering
  // SELinux 'denied { map }' on Android by returning MAP_FAILED immediately with ENODEV.
  // libusb will then cleanly fall back to host heap allocation (malloc).
  if (fd >= 0) {
    struct stat st;
    if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)) {
      errno = ENODEV;
      return MAP_FAILED;
    }
  }
  using MmapFn = void* (*)(void*, size_t, int, int, int, off_t);
  static MmapFn real_mmap = reinterpret_cast<MmapFn>(dlsym(RTLD_NEXT, "mmap"));
  return real_mmap ? real_mmap(addr, length, prot, flags, fd, offset) : MAP_FAILED;
}

__attribute__((visibility("default")))
int ioctl(int fd, int request, ...) {
  va_list args;
  va_start(args, request);
  void* arg = va_arg(args, void*);
  va_end(args);

  using IoctlFn = int (*)(int, int, void*);
  static IoctlFn real_ioctl = reinterpret_cast<IoctlFn>(dlsym(RTLD_NEXT, "ioctl"));
  if (!real_ioctl) return -1;

  if (static_cast<unsigned int>(request) == USBDEVFS_DISCONNECT_CLAIM) {
    int res = real_ioctl(fd, request, arg);
    if (res < 0 && errno == EPERM && arg != nullptr) {
      auto* dc = static_cast<const struct usbdevfs_disconnect_claim*>(arg);
      unsigned int iface = dc->interface;
      LOGI("USBDEVFS_DISCONNECT_CLAIM failed with EPERM for interface %u, falling back to USBDEVFS_CLAIMINTERFACE", iface);
      res = real_ioctl(fd, USBDEVFS_CLAIMINTERFACE, &iface);
      if (res == 0) {
        LOGI("Fallback USBDEVFS_CLAIMINTERFACE succeeded for interface %u", iface);
      } else {
        LOGW("Fallback USBDEVFS_CLAIMINTERFACE failed for interface %u, errno=%d (%s)", iface, errno, strerror(errno));
      }
    }
    return res;
  }

  if (static_cast<unsigned int>(request) == USBDEVFS_IOCTL && arg != nullptr) {
    auto* uctl = static_cast<struct usbdevfs_ioctl*>(arg);
    if (uctl->ioctl_code == USBDEVFS_DISCONNECT) {
      int res = real_ioctl(fd, request, arg);
      if (res < 0 && errno == EPERM) {
        LOGI("USBDEVFS_DISCONNECT failed with EPERM for interface %u, ignoring", uctl->ifno);
        return 0; // Kernel driver cannot be detached without root; ignore
      }
      return res;
    }
  }

  int res = real_ioctl(fd, request, arg);
  if (res < 0 && errno == EPERM) {
    LOGW("ioctl(fd=%d, req=0x%x) returned EPERM", fd, request);
  }
  return res;
}
}

namespace dlvideo {
namespace {

constexpr unsigned kPollIntervalMs = 100;

std::vector<dlsdk_device_handle> getDevices()
{
  unsigned int count = 0;
  if (dlsdk_get_devices(nullptr, &count) != DLSDK_SUCCESS && count == 0) {
    return {};
  }
  if (count == 0) {
    return {};
  }

  std::vector<dlsdk_device_handle> devices(count);
  dlsdk_status status = dlsdk_get_devices(devices.data(), &count);
  while (status == DLSDK_NOT_ENOUGH_SPACE) {
    devices.resize(count);
    status = dlsdk_get_devices(devices.data(), &count);
  }
  if (status != DLSDK_SUCCESS) {
    return {};
  }
  devices.resize(count);
  return devices;
}

std::vector<dlsdk_display_handle> getDisplays(dlsdk_device_handle device)
{
  unsigned int count = 0;
  if (dlsdk_device_get_displays(device, nullptr, &count) != DLSDK_SUCCESS && count == 0) {
    return {};
  }
  if (count == 0) {
    return {};
  }

  std::vector<dlsdk_display_handle> displays(count);
  dlsdk_status status = dlsdk_device_get_displays(device, displays.data(), &count);
  while (status == DLSDK_NOT_ENOUGH_SPACE) {
    displays.resize(count);
    status = dlsdk_device_get_displays(device, displays.data(), &count);
  }
  if (status != DLSDK_SUCCESS) {
    return {};
  }
  displays.resize(count);
  return displays;
}

}  // namespace

dlsdk_status DlsdkSession::initialise()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_initialised) {
    return DLSDK_SUCCESS;
  }

  // Ensure HOME points to a writable directory so libdlsdk can create its internal settings hive (.dl.xml).
  if (getenv("HOME") == nullptr) {
    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir != nullptr) {
      setenv("HOME", tmpdir, 1);
    }
  }

  dlsdk_config config;
  DLSDK_CONFIG_INIT(&config);
  // The application has no permission to scan the USB bus, so devices opened through
  // android.hardware.usb.UsbManager are handed over with dlsdk_usb_attach_device() instead.
  config.flags = DLSDK_CONFIG_FLAG_EXTERNAL_USB_DEVICES;
  config.firmwarePath = nullptr;

  const dlsdk_status status = dlsdk_initialise_with_config(&config);
  if (status != DLSDK_SUCCESS) {
    LOGE("dlsdk_initialise_with_config failed: %d", status);
    return status;
  }

  m_initialised = true;
  LOGI("DisplayLink Direct initialised with external USB devices");
  return DLSDK_SUCCESS;
}

void DlsdkSession::teardown()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_initialised) {
    return;
  }
  m_targets.clear();
  m_layout = WallLayout{};
  dlsdk_teardown();
  m_initialised = false;
  LOGI("DisplayLink Direct torn down");
}

bool DlsdkSession::initialised() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_initialised;
}

dlsdk_status DlsdkSession::attachUsbDevice(int fileDescriptor,
                                           uint16_t busNumber,
                                           uint16_t deviceAddress)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_initialised) {
    return DLSDK_UNSUCCESSFUL;
  }

  dlsdk_usb_device_params params;
  DLSDK_USB_DEVICE_PARAMS_INIT(&params, fileDescriptor, busNumber, deviceAddress);
  const dlsdk_status status = dlsdk_usb_attach_device(&params);
  LOGI("dlsdk_usb_attach_device(fd=%d, bus=%u, address=%u) -> %d",
       fileDescriptor,
       busNumber,
       deviceAddress,
       status);
  return status;
}

dlsdk_status DlsdkSession::detachUsbDevice(uint16_t busNumber, uint16_t deviceAddress)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_initialised) {
    return DLSDK_UNSUCCESSFUL;
  }

  // Handles belonging to the device are invalidated by the detach, so drop the wall first.
  m_targets.clear();
  m_layout = WallLayout{};

  const dlsdk_status status = dlsdk_usb_detach_device(busNumber, deviceAddress);
  LOGI("dlsdk_usb_detach_device(bus=%u, address=%u) -> %d", busNumber, deviceAddress, status);
  return status;
}

dlsdk_status DlsdkSession::collectDisplaysLocked()
{
  m_targets.clear();
  m_layout = WallLayout{};

  for (dlsdk_device_handle device : getDevices()) {
    for (dlsdk_display_handle display : getDisplays(device)) {
      if (display == nullptr) {
        continue;
      }
      const dlsdk_status status = dlsdk_display_power_on(display);
      if (status != DLSDK_SUCCESS) {
        LOGW("dlsdk_display_power_on failed for %s: %d", dlsdk_display_id(display), status);
        continue;
      }
      const dlsdk_rect size = dlsdk_display_size(display);
      const char* id = dlsdk_display_id(display);
      m_targets.push_back(
        WallTarget{display, id ? id : "", size.width, size.height, 0, 0});
    }
  }

  if (m_targets.empty()) {
    return DLSDK_UNSUCCESSFUL_NO_MONITOR;
  }

  // The wall is rendered as a single composite frame buffer, so every display must report the
  // same resolution.
  const unsigned width = m_targets.front().width;
  const unsigned height = m_targets.front().height;
  if (width == 0 || height == 0) {
    return DLSDK_UNSUCCESSFUL_NO_MONITOR;
  }
  for (const WallTarget& target : m_targets) {
    if (target.width != width || target.height != height) {
      LOGE("Displays have mixed resolutions (%ux%u and %ux%u)",
           width,
           height,
           target.width,
           target.height);
      return DLSDK_INVALID_ARGS;
    }
  }

  // Matches the layout used by the samples/video/video-wall.cpp sample: four displays form a 2x2
  // grid, any other count is stacked into a single column.
  const unsigned count = static_cast<unsigned>(m_targets.size());
  m_layout.displayCount = count;
  m_layout.columns = (count == 4) ? 2 : 1;
  m_layout.rows = count / m_layout.columns;
  m_layout.compositeWidth = width * m_layout.columns;
  m_layout.compositeHeight = height * m_layout.rows;
  m_layout.strideBytes = m_layout.compositeWidth * kBytesPerPixel;

  for (unsigned i = 0; i < count; ++i) {
    m_targets[i].column = i % m_layout.columns;
    m_targets[i].row = i / m_layout.columns;
  }

  LOGI("Wall ready: %u display(s) %ux%u arranged %ux%u, composite %ux%u stride %u",
       count,
       width,
       height,
       m_layout.columns,
       m_layout.rows,
       m_layout.compositeWidth,
       m_layout.compositeHeight,
       m_layout.strideBytes);
  return DLSDK_SUCCESS;
}

dlsdk_status DlsdkSession::openWall(unsigned timeoutMs)
{
  // dlsdk_usb_attach_device() only hands the device over; enumeration of the device and its
  // displays completes asynchronously, so poll until something shows up.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  dlsdk_status status = DLSDK_UNSUCCESSFUL_NO_MONITOR;
  m_openCancelled.store(false);

  for (;;) {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (!m_initialised) {
        return DLSDK_UNSUCCESSFUL;
      }
      status = collectDisplaysLocked();
    }
    if (status == DLSDK_SUCCESS || status == DLSDK_INVALID_ARGS) {
      return status;
    }
    if (m_openCancelled.load()) {
      return DLSDK_UNSUCCESSFUL;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return status;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
  }
}

void DlsdkSession::cancelPendingOpen()
{
  m_openCancelled.store(true);
}

void DlsdkSession::closeWall()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  for (const WallTarget& target : m_targets) {
    dlsdk_display_power_off(target.display);
  }
  m_targets.clear();
  m_layout = WallLayout{};
}

bool DlsdkSession::present(const uint8_t* pixels,
                           uint32_t strideBytes,
                           dlsdk_pixel_format format)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_targets.empty() || pixels == nullptr) {
    return false;
  }

  // A DLSDK_BUSY result means the device dropped the frame under resource pressure; resubmitting
  // the same frame once lets it retry with more aggressive compression.
  for (unsigned attempt = 0; attempt < 2; ++attempt) {
    bool submitted = true;
    for (const WallTarget& target : m_targets) {
      const size_t offsetBytes = static_cast<size_t>(target.row) * target.height * strideBytes +
                                 static_cast<size_t>(target.column) * target.width * kBytesPerPixel;
      dlsdk_frame frame;
      DLSDK_FRAME_INIT(&frame, format, pixels + offsetBytes, strideBytes * target.height);
      frame.stride = strideBytes;
      if (dlsdk_display_show(target.display, &frame) != DLSDK_SUCCESS) {
        submitted = false;
      }
    }

    bool busy = false;
    for (const WallTarget& target : m_targets) {
      const dlsdk_status status = dlsdk_display_wait_on_show(target.display);
      if (status == DLSDK_BUSY) {
        busy = true;
      } else if (status != DLSDK_SUCCESS) {
        submitted = false;
      }
    }

    if (!busy) {
      return submitted;
    }
  }
  return false;
}

WallLayout DlsdkSession::layout() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_layout;
}

std::vector<std::string> DlsdkSession::targetIds() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<std::string> ids;
  ids.reserve(m_targets.size());
  for (const WallTarget& target : m_targets) {
    ids.push_back(target.id);
  }
  return ids;
}

}  // namespace dlvideo
