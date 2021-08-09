/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 提供媒体日志记录功能
 */

#include "MediaLog.h"
#include <cstring>
#include <string>
#include <cstdarg>
#ifdef __ANDROID__
#include <unordered_map>
#include <android/log.h>
#endif
#include "MediaLogManager.h"

#ifdef __ANDROID__
namespace {
    std::unordered_map<int, int> LOG_LEVEL_MAP = {
        { LOG_LEVEL_DEBUG, ANDROID_LOG_DEBUG },
        { LOG_LEVEL_INFO, ANDROID_LOG_INFO },
        { LOG_LEVEL_WARN, ANDROID_LOG_WARN },
        { LOG_LEVEL_ERROR, ANDROID_LOG_ERROR },
        { LOG_LEVEL_FATAL, ANDROID_LOG_FATAL },
    };
}
#endif

void SetMediaLogCallback(MediaLogCallbackFunc logCallback)
{
    MediaLogManager::GetInstance().SetLogCallback(logCallback);
}

void MediaLogPrint(int level, const char *tag, const char *fmt, ...)
{
    MediaLogManager &logManager = MediaLogManager::GetInstance();
    if (level < logManager.GetLogLevel() || level > LOG_LEVEL_FATAL || fmt == nullptr) {
        return;
    }
    std::string fullTag = ((tag == nullptr) ? "Media" : ("Media_" + std::string(tag)));

    constexpr int logBufSize = 512;
    char logBuf[logBufSize] = {0};
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(logBuf, logBufSize - 1, fmt, ap);
    va_end(ap);

    if (ret <= 0) {
        return;
    }

    if (static_cast<int64_t>(ret) < static_cast<int64_t>(logBufSize)) {
        logBuf[ret] = '\0';
    }
    if (!logManager.IsLogCallbackNull()) {
        logManager.Callback(level, fullTag, logBuf);
        return;
    }
#ifdef __ANDROID__
    (void) __android_log_write(LOG_LEVEL_MAP[level], fullTag.c_str(), logBuf);
#endif
}
