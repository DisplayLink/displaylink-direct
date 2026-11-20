// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#pragma once

#include <android/surface_texture.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

#include "dlsdk_session.h"
#include "gl_compositor.h"

namespace dlvideo {

struct VideoStats
{
  uint64_t rendered = 0;
  uint64_t presented = 0;
  uint64_t dropped = 0;
};

/// Drives the video wall pipeline.
///
/// A render thread owns the EGL context and scales each SurfaceTexture frame into a free composite
/// buffer. A present thread maps the finished buffer and pushes its regions to the DisplayLink
/// displays. The two run concurrently, so encoding one frame overlaps with rendering the next.
class VideoEngine final
{
public:
  /// Composite buffers in flight: one being presented, one queued and one being rendered.
  static constexpr uint32_t kBufferCount = 3;

  VideoEngine() = default;
  VideoEngine(const VideoEngine&) = delete;
  VideoEngine& operator=(const VideoEngine&) = delete;
  ~VideoEngine();

  /// Starts the pipeline for an already opened wall. Blocks until the render thread has created
  /// its GPU resources, so a false result means the pipeline is not running.
  bool start(DlsdkSession* session,
             ASurfaceTexture* surfaceTexture,
             uint32_t sourceWidth,
             uint32_t sourceHeight);
  void stop();
  bool running() const { return m_running.load(std::memory_order_acquire); }

  /// Called from SurfaceTexture.OnFrameAvailableListener.
  void notifyFrameAvailable();

  /// Updates the source video size used for aspect-ratio preserving scaling.
  void setSourceSize(uint32_t width, uint32_t height);

  VideoStats stats() const;

private:
  void renderLoop();
  void presentLoop();
  void stopLocked();
  CompositeFrame* acquireFreeFrame();
  void releaseFrame(CompositeFrame* frame);
  void publishFrame(CompositeFrame* frame);
  CompositeFrame* acquireReadyFrame();

  DlsdkSession* m_session = nullptr;
  ASurfaceTexture* m_surfaceTexture = nullptr;
  GlCompositor m_compositor;

  std::thread m_renderThread;
  std::thread m_presentThread;
  /// Serialises start() and stop() so the worker threads are never joined twice.
  std::mutex m_lifecycleMutex;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_stopping{false};
  std::atomic<uint32_t> m_sourceSize{0};

  std::mutex m_startMutex;
  std::condition_variable m_startSignal;
  bool m_startComplete = false;
  bool m_startSucceeded = false;

  std::mutex m_frameMutex;
  std::condition_variable m_frameSignal;
  bool m_frameAvailable = false;

  std::mutex m_presentStopMutex;
  std::condition_variable m_presentStopSignal;
  std::atomic<bool> m_presentStopped{false};

  std::mutex m_poolMutex;
  std::condition_variable m_poolSignal;
  std::deque<CompositeFrame*> m_freeFrames;
  CompositeFrame* m_readyFrame = nullptr;

  std::atomic<uint64_t> m_rendered{0};
  std::atomic<uint64_t> m_presented{0};
  std::atomic<uint64_t> m_dropped{0};
};

}  // namespace dlvideo
