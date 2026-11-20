// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#include <jni.h>

#include <android/surface_texture.h>
#include <android/surface_texture_jni.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "dlsdk_session.h"
#include "log.h"
#include "video_engine.h"

namespace {

dlvideo::DlsdkSession g_session;
dlvideo::VideoEngine g_engine;

std::mutex g_surfaceTextureMutex;
ASurfaceTexture* g_surfaceTexture = nullptr;

void releaseSurfaceTexture()
{
  std::lock_guard<std::mutex> lock(g_surfaceTextureMutex);
  if (g_surfaceTexture != nullptr) {
    ASurfaceTexture_release(g_surfaceTexture);
    g_surfaceTexture = nullptr;
  }
}

}  // namespace

extern "C" {

JNIEXPORT jint JNICALL
Java_com_displaylink_video_NativeVideo_nativeInitialise(JNIEnv*, jobject)
{
  return static_cast<jint>(g_session.initialise());
}

JNIEXPORT void JNICALL
Java_com_displaylink_video_NativeVideo_nativeTeardown(JNIEnv*, jobject)
{
  g_session.cancelPendingOpen();
  g_engine.stop();
  releaseSurfaceTexture();
  g_session.teardown();
}

JNIEXPORT jint JNICALL
Java_com_displaylink_video_NativeVideo_nativeAttachUsbDevice(JNIEnv*,
                                                             jobject,
                                                             jint fileDescriptor,
                                                             jint busNumber,
                                                             jint deviceAddress)
{
  return static_cast<jint>(g_session.attachUsbDevice(fileDescriptor,
                                                     static_cast<uint16_t>(busNumber),
                                                     static_cast<uint16_t>(deviceAddress)));
}

JNIEXPORT jint JNICALL
Java_com_displaylink_video_NativeVideo_nativeDetachUsbDevice(JNIEnv*,
                                                             jobject,
                                                             jint busNumber,
                                                             jint deviceAddress)
{
  g_engine.stop();
  releaseSurfaceTexture();
  return static_cast<jint>(g_session.detachUsbDevice(static_cast<uint16_t>(busNumber),
                                                     static_cast<uint16_t>(deviceAddress)));
}

JNIEXPORT jint JNICALL
Java_com_displaylink_video_NativeVideo_nativeOpenWall(JNIEnv*, jobject, jint timeoutMs)
{
  return static_cast<jint>(g_session.openWall(static_cast<unsigned>(timeoutMs)));
}

JNIEXPORT void JNICALL
Java_com_displaylink_video_NativeVideo_nativeCancelOpenWall(JNIEnv*, jobject)
{
  g_session.cancelPendingOpen();
}

JNIEXPORT void JNICALL
Java_com_displaylink_video_NativeVideo_nativeCloseWall(JNIEnv*, jobject)
{
  g_session.cancelPendingOpen();
  g_engine.stop();
  releaseSurfaceTexture();
  g_session.closeWall();
}

/// Returns {displayCount, columns, rows, compositeWidth, compositeHeight, strideBytes, bufferHeight}
/// or null when no wall is open.
JNIEXPORT jintArray JNICALL
Java_com_displaylink_video_NativeVideo_nativeWallLayout(JNIEnv* env, jobject)
{
  const dlvideo::WallLayout layout = g_session.layout();
  if (layout.displayCount == 0) {
    return nullptr;
  }

  const jint values[] = {
    static_cast<jint>(layout.displayCount),
    static_cast<jint>(layout.columns),
    static_cast<jint>(layout.rows),
    static_cast<jint>(layout.compositeWidth),
    static_cast<jint>(layout.compositeHeight),
    static_cast<jint>(layout.strideBytes),
    static_cast<jint>(layout.compositeHeight + dlvideo::DlsdkSession::kSlackRows),
  };
  const jsize count = static_cast<jsize>(sizeof(values) / sizeof(values[0]));

  jintArray result = env->NewIntArray(count);
  if (result != nullptr) {
    env->SetIntArrayRegion(result, 0, count, values);
  }
  return result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_displaylink_video_NativeVideo_nativeDisplayIds(JNIEnv* env, jobject)
{
  const std::vector<std::string> ids = g_session.targetIds();
  jclass stringClass = env->FindClass("java/lang/String");
  if (stringClass == nullptr) {
    return nullptr;
  }

  jobjectArray result =
    env->NewObjectArray(static_cast<jsize>(ids.size()), stringClass, nullptr);
  if (result == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < ids.size(); ++i) {
    jstring id = env->NewStringUTF(ids[i].c_str());
    env->SetObjectArrayElement(result, static_cast<jsize>(i), id);
    env->DeleteLocalRef(id);
  }
  return result;
}

JNIEXPORT jboolean JNICALL
Java_com_displaylink_video_NativeVideo_nativeStartPlayback(JNIEnv* env,
                                                           jobject,
                                                           jobject surfaceTexture,
                                                           jint sourceWidth,
                                                           jint sourceHeight)
{
  g_engine.stop();
  releaseSurfaceTexture();

  ASurfaceTexture* nativeSurfaceTexture =
    ASurfaceTexture_fromSurfaceTexture(env, surfaceTexture);
  if (nativeSurfaceTexture == nullptr) {
    LOGE("ASurfaceTexture_fromSurfaceTexture failed");
    return JNI_FALSE;
  }

  {
    std::lock_guard<std::mutex> lock(g_surfaceTextureMutex);
    g_surfaceTexture = nativeSurfaceTexture;
  }

  if (!g_engine.start(&g_session,
                      nativeSurfaceTexture,
                      static_cast<uint32_t>(sourceWidth),
                      static_cast<uint32_t>(sourceHeight))) {
    releaseSurfaceTexture();
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_displaylink_video_NativeVideo_nativeStopPlayback(JNIEnv*, jobject)
{
  g_engine.stop();
  releaseSurfaceTexture();
}

JNIEXPORT void JNICALL
Java_com_displaylink_video_NativeVideo_nativeOnFrameAvailable(JNIEnv*, jobject)
{
  g_engine.notifyFrameAvailable();
}

JNIEXPORT void JNICALL
Java_com_displaylink_video_NativeVideo_nativeSetSourceSize(JNIEnv*,
                                                           jobject,
                                                           jint width,
                                                           jint height)
{
  g_engine.setSourceSize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

/// Shows a single pre-composed still frame. The buffer must be a direct ByteBuffer holding
/// ARGB_8888 pixels, which are laid out in memory as R, G, B, X - DLSDK_PIXEL_FORMAT_XBGR.
JNIEXPORT jboolean JNICALL
Java_com_displaylink_video_NativeVideo_nativeShowStillFrame(JNIEnv* env,
                                                            jobject,
                                                            jobject pixelBuffer,
                                                            jint strideBytes)
{
  auto* pixels = static_cast<const uint8_t*>(env->GetDirectBufferAddress(pixelBuffer));
  if (pixels == nullptr) {
    LOGE("nativeShowStillFrame requires a direct ByteBuffer");
    return JNI_FALSE;
  }
  return g_session.present(pixels, static_cast<uint32_t>(strideBytes), DLSDK_PIXEL_FORMAT_XBGR)
           ? JNI_TRUE
           : JNI_FALSE;
}

/// Returns {rendered, presented, dropped}.
JNIEXPORT jlongArray JNICALL
Java_com_displaylink_video_NativeVideo_nativeStats(JNIEnv* env, jobject)
{
  const dlvideo::VideoStats stats = g_engine.stats();
  const jlong values[] = {static_cast<jlong>(stats.rendered),
                          static_cast<jlong>(stats.presented),
                          static_cast<jlong>(stats.dropped)};
  const jsize count = static_cast<jsize>(sizeof(values) / sizeof(values[0]));

  jlongArray result = env->NewLongArray(count);
  if (result != nullptr) {
    env->SetLongArrayRegion(result, 0, count, values);
  }
  return result;
}

}  // extern "C"
