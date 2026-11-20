// Copyright (c) 2026 Synaptics Incorporated
// SPDX-License-Identifier: MIT
#pragma once

#include <android/log.h>

#define DLVIDEO_LOG_TAG "dlvideo"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, DLVIDEO_LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, DLVIDEO_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, DLVIDEO_LOG_TAG, __VA_ARGS__)
