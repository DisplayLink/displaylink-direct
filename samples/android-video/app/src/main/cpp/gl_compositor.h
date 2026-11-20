// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
// gl2ext.h relies on the core GLES types, so the core header has to come first.
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>
#include <android/surface_texture.h>

#include <cstdint>
#include <vector>

namespace dlvideo {

/// A GPU-written, CPU-readable composite frame buffer.
///
/// Rendering targets an AHardwareBuffer through an EGLImage, so the pixels the DisplayLink encoder
/// reads are the pixels the GPU wrote - there is no glReadPixels copy in the pipeline.
struct CompositeFrame
{
  AHardwareBuffer* buffer = nullptr;
  EGLImageKHR image = EGL_NO_IMAGE_KHR;
  GLuint texture = 0;
  GLuint framebuffer = 0;
  uint32_t strideBytes = 0;
};

/// Scales video frames arriving on a SurfaceTexture into a composite frame buffer laid out for the
/// DisplayLink wall.
///
/// All methods must be called on the thread that called initialise(), which owns the EGL context.
class GlCompositor final
{
public:
  GlCompositor() = default;
  GlCompositor(const GlCompositor&) = delete;
  GlCompositor& operator=(const GlCompositor&) = delete;
  ~GlCompositor();

  /// Creates the EGL context, the shader program and poolSize composite frame buffers of
  /// width x (height + slackRows) pixels.
  bool initialise(uint32_t width, uint32_t height, uint32_t slackRows, uint32_t poolSize);
  void shutdown();

  /// Binds surfaceTexture to the compositor's external texture. Ownership stays with the caller.
  bool attachSurfaceTexture(ASurfaceTexture* surfaceTexture);

  /// Consumes the most recent SurfaceTexture frame and scales it into target, preserving the
  /// aspect ratio of sourceWidth x sourceHeight. Blocks until the GPU has finished writing.
  bool renderSurfaceTexture(CompositeFrame& target, uint32_t sourceWidth, uint32_t sourceHeight);

  const std::vector<CompositeFrame*>& frames() const { return m_framePointers; }
  uint32_t width() const { return m_width; }
  uint32_t height() const { return m_height; }

private:
  bool createContext();
  bool createProgram();
  bool createFrame(CompositeFrame& frame, uint32_t width, uint32_t height);
  void destroyFrame(CompositeFrame& frame);

  EGLDisplay m_display = EGL_NO_DISPLAY;
  EGLContext m_context = EGL_NO_CONTEXT;
  EGLSurface m_surface = EGL_NO_SURFACE;

  GLuint m_program = 0;
  GLint m_positionLocation = -1;
  GLint m_texCoordLocation = -1;
  GLint m_texMatrixLocation = -1;
  GLint m_textureLocation = -1;
  GLuint m_externalTexture = 0;

  ASurfaceTexture* m_surfaceTexture = nullptr;
  bool m_surfaceTextureAttached = false;

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  std::vector<CompositeFrame> m_frames;
  std::vector<CompositeFrame*> m_framePointers;
};

/// Maps a composite frame for CPU reads for as long as the guard is alive.
class CompositeFrameLock final
{
public:
  CompositeFrameLock() = default;
  CompositeFrameLock(const CompositeFrameLock&) = delete;
  CompositeFrameLock& operator=(const CompositeFrameLock&) = delete;
  ~CompositeFrameLock();

  bool lock(CompositeFrame& frame);
  void unlock();
  const uint8_t* pixels() const { return static_cast<const uint8_t*>(m_pixels); }

private:
  AHardwareBuffer* m_buffer = nullptr;
  void* m_pixels = nullptr;
};

}  // namespace dlvideo
