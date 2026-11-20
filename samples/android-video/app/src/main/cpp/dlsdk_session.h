// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#pragma once

#include <dlsdk/dlsdk.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace dlvideo {

/// One DisplayLink output taking part in the wall, and its position in the composite grid.
struct WallTarget
{
  dlsdk_display_handle display = nullptr;
  std::string id;
  unsigned width = 0;
  unsigned height = 0;
  unsigned column = 0;
  unsigned row = 0;
};

/// Geometry of the composite frame buffer that the whole wall is rendered into.
struct WallLayout
{
  unsigned columns = 0;
  unsigned rows = 0;
  unsigned compositeWidth = 0;
  unsigned compositeHeight = 0;
  unsigned strideBytes = 0;
  unsigned displayCount = 0;
};

/// Owns the DisplayLink Direct system, the externally supplied USB devices, and the powered-on
/// displays that make up the wall.
class DlsdkSession final
{
public:
  static constexpr unsigned kBytesPerPixel = 4;

  /// Number of spare rows appended to the composite frame buffer.
  ///
  /// dlsdk_display_show() requires pixelsSize to be at least stride * displayHeight. For a 2x2
  /// wall the bottom-right region starts one display row into the last composite row band, so the
  /// declared size reaches one display row beyond the composite area. The spare row keeps that
  /// range inside the real allocation.
  static constexpr unsigned kSlackRows = 1;

  DlsdkSession() = default;
  DlsdkSession(const DlsdkSession&) = delete;
  DlsdkSession& operator=(const DlsdkSession&) = delete;

  dlsdk_status initialise();
  void teardown();
  bool initialised() const;

  dlsdk_status attachUsbDevice(int fileDescriptor, uint16_t busNumber, uint16_t deviceAddress);
  dlsdk_status detachUsbDevice(uint16_t busNumber, uint16_t deviceAddress);

  /// Waits for attached devices to enumerate, powers on every display and computes the wall
  /// layout. Returns DLSDK_UNSUCCESSFUL_NO_MONITOR when nothing enumerated within timeoutMs, and
  /// DLSDK_INVALID_ARGS when the displays do not all report the same resolution.
  dlsdk_status openWall(unsigned timeoutMs);

  /// Makes an in-flight openWall() give up at its next poll, so shutdown does not have to wait
  /// out the full timeout.
  void cancelPendingOpen();

  /// Powers off the displays and releases the wall layout, leaving the USB devices attached.
  void closeWall();

  /// Submits one composite frame to every display and blocks until all of them have shown it.
  ///
  /// The region offsets are derived from strideBytes rather than from the nominal layout stride,
  /// because a GPU allocation may pad its rows.
  bool present(const uint8_t* pixels, uint32_t strideBytes, dlsdk_pixel_format format);

  WallLayout layout() const;
  std::vector<std::string> targetIds() const;

private:
  dlsdk_status collectDisplaysLocked();

  mutable std::mutex m_mutex;
  std::atomic<bool> m_openCancelled{false};
  bool m_initialised = false;
  std::vector<WallTarget> m_targets;
  WallLayout m_layout;
};

}  // namespace dlvideo
