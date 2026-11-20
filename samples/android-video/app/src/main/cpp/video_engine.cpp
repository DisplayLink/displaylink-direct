// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#include "video_engine.h"

#include <chrono>

#include "log.h"

namespace dlvideo {
namespace {

constexpr auto kStatsInterval = std::chrono::seconds(5);

uint32_t packSize(uint32_t width, uint32_t height)
{
  return ((width & 0xFFFFU) << 16U) | (height & 0xFFFFU);
}

}  // namespace

VideoEngine::~VideoEngine()
{
  stop();
}

void VideoEngine::setSourceSize(uint32_t width, uint32_t height)
{
  m_sourceSize.store(packSize(width, height), std::memory_order_release);
}

bool VideoEngine::start(DlsdkSession* session,
                       ASurfaceTexture* surfaceTexture,
                       uint32_t sourceWidth,
                       uint32_t sourceHeight)
{
  if (session == nullptr || surfaceTexture == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lifecycle(m_lifecycleMutex);
  // Leaves any previous pipeline fully joined, so the thread objects can be reassigned.
  stopLocked();

  const WallLayout layout = session->layout();
  if (layout.compositeWidth == 0 || layout.compositeHeight == 0) {
    LOGE("Cannot start the wall before any display has been opened");
    return false;
  }

  m_session = session;
  m_surfaceTexture = surfaceTexture;
  setSourceSize(sourceWidth, sourceHeight);
  m_stopping.store(false);
  m_presentStopped.store(false);
  m_frameAvailable = false;
  m_startComplete = false;
  m_startSucceeded = false;
  m_rendered.store(0);
  m_presented.store(0);
  m_dropped.store(0);

  m_renderThread = std::thread(&VideoEngine::renderLoop, this);

  std::unique_lock<std::mutex> lock(m_startMutex);
  m_startSignal.wait(lock, [this] { return m_startComplete; });
  const bool succeeded = m_startSucceeded;
  lock.unlock();

  if (!succeeded) {
    m_stopping.store(true);
    {
      std::lock_guard<std::mutex> frameLock(m_frameMutex);
    }
    m_frameSignal.notify_all();
    {
      std::lock_guard<std::mutex> stopLock(m_presentStopMutex);
      m_presentStopped.store(true);
    }
    m_presentStopSignal.notify_all();
    if (m_renderThread.joinable()) {
      m_renderThread.join();
    }
    m_session = nullptr;
    m_surfaceTexture = nullptr;
    return false;
  }

  m_running.store(true, std::memory_order_release);
  m_presentThread = std::thread(&VideoEngine::presentLoop, this);
  return true;
}

void VideoEngine::stop()
{
  std::lock_guard<std::mutex> lifecycle(m_lifecycleMutex);
  stopLocked();
}

void VideoEngine::stopLocked()
{
  if (!m_renderThread.joinable() && !m_presentThread.joinable()) {
    return;
  }

  m_stopping.store(true);
  m_running.store(false, std::memory_order_release);
  // Taking each mutex before notifying closes the window where a worker has evaluated its
  // predicate but has not started waiting yet, which would otherwise lose the wakeup.
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
  }
  m_frameSignal.notify_all();
  {
    std::lock_guard<std::mutex> lock(m_poolMutex);
  }
  m_poolSignal.notify_all();

  if (m_presentThread.joinable()) {
    m_presentThread.join();
  }

  // With presentThread joined, no more frames are being read by DisplayLink Direct.
  // Allow renderLoop to proceed with tearing down the compositor and freeing buffers.
  {
    std::lock_guard<std::mutex> lock(m_presentStopMutex);
    m_presentStopped.store(true);
  }
  m_presentStopSignal.notify_all();

  if (m_renderThread.joinable()) {
    m_renderThread.join();
  }

  {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    m_freeFrames.clear();
    m_readyFrame = nullptr;
  }
  m_session = nullptr;
  m_surfaceTexture = nullptr;
  LOGI("Wall engine stopped");
}

void VideoEngine::notifyFrameAvailable()
{
  {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_frameAvailable = true;
  }
  m_frameSignal.notify_one();
}

CompositeFrame* VideoEngine::acquireFreeFrame()
{
  std::unique_lock<std::mutex> lock(m_poolMutex);
  m_poolSignal.wait(lock, [this] { return !m_freeFrames.empty() || m_stopping.load(); });
  if (m_freeFrames.empty()) {
    return nullptr;
  }
  CompositeFrame* frame = m_freeFrames.front();
  m_freeFrames.pop_front();
  return frame;
}

void VideoEngine::releaseFrame(CompositeFrame* frame)
{
  if (frame == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    m_freeFrames.push_back(frame);
  }
  m_poolSignal.notify_all();
}

void VideoEngine::publishFrame(CompositeFrame* frame)
{
  CompositeFrame* superseded = nullptr;
  {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    // Only the newest finished frame is worth showing; an older queued frame is dropped so the
    // wall stays in step with playback instead of falling behind.
    superseded = m_readyFrame;
    m_readyFrame = frame;
    if (superseded != nullptr) {
      m_freeFrames.push_back(superseded);
      m_dropped.fetch_add(1, std::memory_order_relaxed);
    }
  }
  m_poolSignal.notify_all();
}

CompositeFrame* VideoEngine::acquireReadyFrame()
{
  std::unique_lock<std::mutex> lock(m_poolMutex);
  m_poolSignal.wait(lock, [this] { return m_readyFrame != nullptr || m_stopping.load(); });
  CompositeFrame* frame = m_readyFrame;
  m_readyFrame = nullptr;
  return frame;
}

void VideoEngine::renderLoop()
{
  const WallLayout layout = m_session->layout();
  const bool initialised = m_compositor.initialise(layout.compositeWidth,
                                                   layout.compositeHeight,
                                                   DlsdkSession::kSlackRows,
                                                   kBufferCount) &&
                           m_compositor.attachSurfaceTexture(m_surfaceTexture);

  if (initialised) {
    std::lock_guard<std::mutex> lock(m_poolMutex);
    for (CompositeFrame* frame : m_compositor.frames()) {
      m_freeFrames.push_back(frame);
    }
  }

  {
    std::lock_guard<std::mutex> lock(m_startMutex);
    m_startComplete = true;
    m_startSucceeded = initialised;
  }
  m_startSignal.notify_all();

  if (!initialised) {
    m_compositor.shutdown();
    return;
  }

  while (!m_stopping.load()) {
    {
      std::unique_lock<std::mutex> lock(m_frameMutex);
      m_frameSignal.wait(lock, [this] { return m_frameAvailable || m_stopping.load(); });
      // Coalesce: any frames that arrived while the previous one was rendering are superseded by
      // the newest one, which is what updateTexImage() will hand back.
      m_frameAvailable = false;
    }
    if (m_stopping.load()) {
      break;
    }

    CompositeFrame* frame = acquireFreeFrame();
    if (frame == nullptr) {
      break;
    }

    const uint32_t sourceSize = m_sourceSize.load(std::memory_order_acquire);
    if (m_compositor.renderSurfaceTexture(*frame, sourceSize >> 16U, sourceSize & 0xFFFFU)) {
      m_rendered.fetch_add(1, std::memory_order_relaxed);
      publishFrame(frame);
    } else {
      releaseFrame(frame);
    }
  }

  // Do not shut down the compositor and destroy AHardwareBuffers until presentLoop
  // has completely finished presenting and joined.
  {
    std::unique_lock<std::mutex> lock(m_presentStopMutex);
    m_presentStopSignal.wait(lock, [this] { return m_presentStopped.load(); });
  }

  m_compositor.shutdown();
}

void VideoEngine::presentLoop()
{
  auto lastStats = std::chrono::steady_clock::now();
  uint64_t lastPresented = 0;

  while (!m_stopping.load()) {
    CompositeFrame* frame = acquireReadyFrame();
    if (frame == nullptr) {
      break;
    }

    {
      CompositeFrameLock mapping;
      if (mapping.lock(*frame)) {
        if (m_session->present(mapping.pixels(), frame->strideBytes, DLSDK_PIXEL_FORMAT_XRGB)) {
          m_presented.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
    releaseFrame(frame);

    const auto now = std::chrono::steady_clock::now();
    if (now - lastStats >= kStatsInterval) {
      const uint64_t presented = m_presented.load(std::memory_order_relaxed);
      const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStats).count();
      LOGI("Wall: %.1f fps presented, %llu rendered, %llu dropped",
           elapsed > 0 ? (static_cast<double>(presented - lastPresented) * 1000.0 /
                          static_cast<double>(elapsed))
                       : 0.0,
           static_cast<unsigned long long>(m_rendered.load(std::memory_order_relaxed)),
           static_cast<unsigned long long>(m_dropped.load(std::memory_order_relaxed)));
      lastStats = now;
      lastPresented = presented;
    }
  }
}

VideoStats VideoEngine::stats() const
{
  VideoStats stats;
  stats.rendered = m_rendered.load(std::memory_order_relaxed);
  stats.presented = m_presented.load(std::memory_order_relaxed);
  stats.dropped = m_dropped.load(std::memory_order_relaxed);
  return stats;
}

}  // namespace dlvideo
