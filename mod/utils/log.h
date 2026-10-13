#pragma once

#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "XYAZL", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "XYAZL", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "XYAZL", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "XYAZL", __VA_ARGS__)
