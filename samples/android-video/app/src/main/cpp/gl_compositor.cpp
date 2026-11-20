// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#include "gl_compositor.h"

#include <algorithm>
#include <cstring>

#include "log.h"

namespace dlvideo {
namespace {

// Loaded through eglGetProcAddress(): EGL_ANDROID_get_native_client_buffer, EGL_KHR_image_base and
// GL_OES_EGL_image are extensions rather than core entry points.
PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC g_eglGetNativeClientBuffer = nullptr;
PFNEGLCREATEIMAGEKHRPROC g_eglCreateImage = nullptr;
PFNEGLDESTROYIMAGEKHRPROC g_eglDestroyImage = nullptr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC g_glEGLImageTargetTexture2D = nullptr;

bool loadExtensions()
{
  if (g_eglGetNativeClientBuffer != nullptr) {
    return true;
  }
  g_eglGetNativeClientBuffer = reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(
    eglGetProcAddress("eglGetNativeClientBufferANDROID"));
  g_eglCreateImage =
    reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
  g_eglDestroyImage =
    reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
  g_glEGLImageTargetTexture2D = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
    eglGetProcAddress("glEGLImageTargetTexture2DOES"));

  if (g_eglGetNativeClientBuffer == nullptr || g_eglCreateImage == nullptr ||
      g_eglDestroyImage == nullptr || g_glEGLImageTargetTexture2D == nullptr) {
    LOGE("Required EGL/GL image extensions are unavailable");
    g_eglGetNativeClientBuffer = nullptr;
    return false;
  }
  return true;
}

// The vertex shader flips Y because GL treats row zero of the render target as the bottom of the
// image, while the DisplayLink encoder reads row zero as the top.
const char* kVertexShader = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
uniform mat4 uTexMatrix;
varying highp vec2 vTexCoord;
void main() {
  gl_Position = vec4(aPosition.x, -aPosition.y, 0.0, 1.0);
  vTexCoord = (uTexMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
}
)";

// The .bgra swizzle writes the bytes in B, G, R, X order, which is DLSDK_PIXEL_FORMAT_XRGB.
const char* kFragmentShader = R"(
#extension GL_OES_EGL_image_external : require
precision highp float;
varying highp vec2 vTexCoord;
uniform samplerExternalOES uTexture;
void main() {
  gl_FragColor = texture2D(uTexture, vTexCoord).bgra;
}
)";

GLuint compileShader(GLenum type, const char* source)
{
  const GLuint shader = glCreateShader(type);
  if (shader == 0) {
    return 0;
  }
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled != GL_TRUE) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> infoLog(static_cast<size_t>(std::max(length, 1)));
    glGetShaderInfoLog(shader, length, nullptr, infoLog.data());
    LOGE("Shader compilation failed: %s", infoLog.data());
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

GlCompositor::~GlCompositor()
{
  shutdown();
}

bool GlCompositor::createContext()
{
  m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (m_display == EGL_NO_DISPLAY) {
    LOGE("eglGetDisplay failed");
    return false;
  }
  if (eglInitialize(m_display, nullptr, nullptr) != EGL_TRUE) {
    LOGE("eglInitialize failed: 0x%x", eglGetError());
    return false;
  }

  const EGLint configAttribs[] = {EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES3_BIT,
                                  EGL_SURFACE_TYPE,
                                  EGL_PBUFFER_BIT,
                                  EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_ALPHA_SIZE,
                                  8,
                                  EGL_NONE};
  EGLConfig config = nullptr;
  EGLint configCount = 0;
  if (eglChooseConfig(m_display, configAttribs, &config, 1, &configCount) != EGL_TRUE ||
      configCount == 0) {
    LOGE("eglChooseConfig failed: 0x%x", eglGetError());
    return false;
  }

  const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, contextAttribs);
  if (m_context == EGL_NO_CONTEXT) {
    LOGE("eglCreateContext failed: 0x%x", eglGetError());
    return false;
  }

  // Rendering only ever targets framebuffer objects, so a 1x1 pbuffer is enough to make the
  // context current on devices without EGL_KHR_surfaceless_context.
  const EGLint surfaceAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  m_surface = eglCreatePbufferSurface(m_display, config, surfaceAttribs);
  if (m_surface == EGL_NO_SURFACE) {
    LOGE("eglCreatePbufferSurface failed: 0x%x", eglGetError());
    return false;
  }

  if (eglMakeCurrent(m_display, m_surface, m_surface, m_context) != EGL_TRUE) {
    LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
    return false;
  }
  return true;
}

bool GlCompositor::createProgram()
{
  const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, kVertexShader);
  if (vertexShader == 0) {
    return false;
  }
  const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
  if (fragmentShader == 0) {
    glDeleteShader(vertexShader);
    return false;
  }

  m_program = glCreateProgram();
  glAttachShader(m_program, vertexShader);
  glAttachShader(m_program, fragmentShader);
  glLinkProgram(m_program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  GLint linked = GL_FALSE;
  glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    GLint length = 0;
    glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> infoLog(static_cast<size_t>(std::max(length, 1)));
    glGetProgramInfoLog(m_program, length, nullptr, infoLog.data());
    LOGE("Program link failed: %s", infoLog.data());
    glDeleteProgram(m_program);
    m_program = 0;
    return false;
  }

  m_positionLocation = glGetAttribLocation(m_program, "aPosition");
  m_texCoordLocation = glGetAttribLocation(m_program, "aTexCoord");
  m_texMatrixLocation = glGetUniformLocation(m_program, "uTexMatrix");
  m_textureLocation = glGetUniformLocation(m_program, "uTexture");

  glGenTextures(1, &m_externalTexture);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_externalTexture);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  return true;
}

bool GlCompositor::createFrame(CompositeFrame& frame, uint32_t width, uint32_t height)
{
  AHardwareBuffer_Desc desc = {};
  desc.width = width;
  desc.height = height;
  desc.layers = 1;
  desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
               AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
               AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;

  if (AHardwareBuffer_allocate(&desc, &frame.buffer) != 0 || frame.buffer == nullptr) {
    LOGE("AHardwareBuffer_allocate failed for %ux%u", width, height);
    return false;
  }

  AHardwareBuffer_Desc actual = {};
  AHardwareBuffer_describe(frame.buffer, &actual);
  frame.strideBytes = actual.stride * 4;

  EGLClientBuffer clientBuffer = g_eglGetNativeClientBuffer(frame.buffer);
  if (clientBuffer == nullptr) {
    LOGE("eglGetNativeClientBufferANDROID failed");
    return false;
  }

  const EGLint imageAttribs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
  frame.image = g_eglCreateImage(
    m_display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, imageAttribs);
  if (frame.image == EGL_NO_IMAGE_KHR) {
    LOGE("eglCreateImageKHR failed: 0x%x", eglGetError());
    return false;
  }

  glGenTextures(1, &frame.texture);
  glBindTexture(GL_TEXTURE_2D, frame.texture);
  g_glEGLImageTargetTexture2D(GL_TEXTURE_2D, static_cast<GLeglImageOES>(frame.image));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &frame.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
  glFramebufferTexture2D(
    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame.texture, 0);
  const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
    LOGE("Framebuffer incomplete: 0x%x", fboStatus);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
  }

  // Start from a fully black composite so letterbox bars and the spare rows are defined.
  glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
  glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return true;
}

void GlCompositor::destroyFrame(CompositeFrame& frame)
{
  if (frame.framebuffer != 0) {
    glDeleteFramebuffers(1, &frame.framebuffer);
    frame.framebuffer = 0;
  }
  if (frame.texture != 0) {
    glDeleteTextures(1, &frame.texture);
    frame.texture = 0;
  }
  if (frame.image != EGL_NO_IMAGE_KHR && g_eglDestroyImage != nullptr) {
    g_eglDestroyImage(m_display, frame.image);
    frame.image = EGL_NO_IMAGE_KHR;
  }
  if (frame.buffer != nullptr) {
    AHardwareBuffer_release(frame.buffer);
    frame.buffer = nullptr;
  }
}

bool GlCompositor::initialise(uint32_t width,
                              uint32_t height,
                              uint32_t slackRows,
                              uint32_t poolSize)
{
  if (width == 0 || height == 0 || poolSize == 0) {
    return false;
  }

  m_width = width;
  m_height = height;

  if (!createContext()) {
    shutdown();
    return false;
  }
  if (!loadExtensions()) {
    shutdown();
    return false;
  }
  if (!createProgram()) {
    shutdown();
    return false;
  }

  m_frames.resize(poolSize);
  for (CompositeFrame& frame : m_frames) {
    if (!createFrame(frame, width, height + slackRows)) {
      shutdown();
      return false;
    }
  }
  m_framePointers.clear();
  m_framePointers.reserve(poolSize);
  for (CompositeFrame& frame : m_frames) {
    m_framePointers.push_back(&frame);
  }

  LOGI("Compositor ready: %ux%u (+%u spare rows), %u buffers, stride %u bytes",
       width,
       height,
       slackRows,
       poolSize,
       m_frames.front().strideBytes);
  return true;
}

void GlCompositor::shutdown()
{
  if (m_display != EGL_NO_DISPLAY && m_context != EGL_NO_CONTEXT) {
    eglMakeCurrent(m_display, m_surface, m_surface, m_context);

    if (m_surfaceTextureAttached && m_surfaceTexture != nullptr) {
      ASurfaceTexture_detachFromGLContext(m_surfaceTexture);
      m_surfaceTextureAttached = false;
    }
    for (CompositeFrame& frame : m_frames) {
      destroyFrame(frame);
    }
    if (m_externalTexture != 0) {
      glDeleteTextures(1, &m_externalTexture);
      m_externalTexture = 0;
    }
    if (m_program != 0) {
      glDeleteProgram(m_program);
      m_program = 0;
    }
  }
  m_frames.clear();
  m_framePointers.clear();
  m_surfaceTexture = nullptr;

  if (m_display != EGL_NO_DISPLAY) {
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_surface != EGL_NO_SURFACE) {
      eglDestroySurface(m_display, m_surface);
      m_surface = EGL_NO_SURFACE;
    }
    if (m_context != EGL_NO_CONTEXT) {
      eglDestroyContext(m_display, m_context);
      m_context = EGL_NO_CONTEXT;
    }
    eglTerminate(m_display);
    m_display = EGL_NO_DISPLAY;
  }
}

bool GlCompositor::attachSurfaceTexture(ASurfaceTexture* surfaceTexture)
{
  if (surfaceTexture == nullptr || m_externalTexture == 0) {
    return false;
  }
  if (ASurfaceTexture_attachToGLContext(surfaceTexture, m_externalTexture) != 0) {
    LOGE("ASurfaceTexture_attachToGLContext failed");
    return false;
  }
  m_surfaceTexture = surfaceTexture;
  m_surfaceTextureAttached = true;
  return true;
}

bool GlCompositor::renderSurfaceTexture(CompositeFrame& target,
                                        uint32_t sourceWidth,
                                        uint32_t sourceHeight)
{
  if (m_surfaceTexture == nullptr || !m_surfaceTextureAttached) {
    return false;
  }
  if (ASurfaceTexture_updateTexImage(m_surfaceTexture) != 0) {
    return false;
  }

  float texMatrix[16] = {};
  ASurfaceTexture_getTransformMatrix(m_surfaceTexture, texMatrix);

  // Letterbox the source inside the composite so the wall keeps the video's aspect ratio.
  float scaleX = 1.0F;
  float scaleY = 1.0F;
  if (sourceWidth > 0 && sourceHeight > 0 && m_width > 0 && m_height > 0) {
    const float sourceAspect = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
    const float targetAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    if (sourceAspect > targetAspect) {
      scaleY = targetAspect / sourceAspect;
    } else if (sourceAspect < targetAspect) {
      scaleX = sourceAspect / targetAspect;
    }
  }

  const GLfloat positions[] = {
    -scaleX, -scaleY,
    scaleX,  -scaleY,
    -scaleX, scaleY,
    scaleX,  scaleY,
  };
  const GLfloat texCoords[] = {
    0.0F, 0.0F,
    1.0F, 0.0F,
    0.0F, 1.0F,
    1.0F, 1.0F,
  };

  glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
  glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(m_program);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_externalTexture);
  glUniform1i(m_textureLocation, 0);
  glUniformMatrix4fv(m_texMatrixLocation, 1, GL_FALSE, texMatrix);

  glEnableVertexAttribArray(static_cast<GLuint>(m_positionLocation));
  glVertexAttribPointer(
    static_cast<GLuint>(m_positionLocation), 2, GL_FLOAT, GL_FALSE, 0, positions);
  glEnableVertexAttribArray(static_cast<GLuint>(m_texCoordLocation));
  glVertexAttribPointer(
    static_cast<GLuint>(m_texCoordLocation), 2, GL_FLOAT, GL_FALSE, 0, texCoords);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableVertexAttribArray(static_cast<GLuint>(m_positionLocation));
  glDisableVertexAttribArray(static_cast<GLuint>(m_texCoordLocation));
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // The encoder reads these pixels from the CPU, so the GPU writes have to have landed first.
  const GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (fence != nullptr) {
    glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    glDeleteSync(fence);
  } else {
    glFinish();
  }
  return true;
}

CompositeFrameLock::~CompositeFrameLock()
{
  unlock();
}

bool CompositeFrameLock::lock(CompositeFrame& frame)
{
  unlock();
  if (frame.buffer == nullptr) {
    return false;
  }
  if (AHardwareBuffer_lock(
        frame.buffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &m_pixels) != 0) {
    LOGE("AHardwareBuffer_lock failed");
    m_pixels = nullptr;
    return false;
  }
  m_buffer = frame.buffer;
  return true;
}

void CompositeFrameLock::unlock()
{
  if (m_buffer != nullptr) {
    AHardwareBuffer_unlock(m_buffer, nullptr);
    m_buffer = nullptr;
  }
  m_pixels = nullptr;
}

}  // namespace dlvideo
