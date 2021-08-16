/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 提供媒体日志记录功能
 */

#include "MediaLog.h"
#include <cstring>
#include <string>
#include <cstdarg>
#include <unordered_map>
#ifdef __ANDROID__
#include <android/log.h>
#else
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <sys/syscall.h>
#ifdef SYS_gettid
#define GetTid() syscall(SYS_gettid)
#else
#define GetTid() (0)
#endif
#endif
#include "MediaLogManager.h"

#ifdef __ANDROID__
namespace {
    std::unordered_map<int, int> g_logLevelMap = {
        { LOG_LEVEL_DEBUG, ANDROID_LOG_DEBUG },
        { LOG_LEVEL_INFO, ANDROID_LOG_INFO },
        { LOG_LEVEL_WARN, ANDROID_LOG_WARN },
        { LOG_LEVEL_ERROR, ANDROID_LOG_ERROR },
        { LOG_LEVEL_FATAL, ANDROID_LOG_FATAL },
    };
}
#else
namespace {
    std::unordered_map<int, std::string> g_logLevelMap = {
        { LOG_LEVEL_DEBUG, "D" },
        { LOG_LEVEL_INFO, "I" },
        { LOG_LEVEL_WARN, "W" },
        { LOG_LEVEL_ERROR, "E" },
        { LOG_LEVEL_FATAL, "F" },
    };
    constexpr uint32_t TIME_LENGTH = 128;
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
    (void) __android_log_write(g_logLevelMap[level], fullTag.c_str(), logBuf);
#else
    auto nowTimePoint = std::chrono::system_clock::now();
    auto nowTimeMs = std::chrono::time_point_cast<std::chrono::microseconds>(nowTimePoint);
    time_t nowTime = std::chrono::system_clock::to_time_t(nowTimePoint);
    struct tm *time = localtime(&nowTime);
    if (time == nullptr) {
        printf("localtime get failed");
        return;
    }
    char timeStampBuf[TIME_LENGTH] = {0};
    int err = sprintf(timeStampBuf, "[%02d-%02d %02d:%02d:%02d.%03ld]",
        ++(time->tm_mon), time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec,
        nowTimeMs.time_since_epoch().count() % 1000);
    if (err < 0) {
        printf("sprintf failed: %d", err);
        return;
    }
    printf("%s %d %ld %s %s: %s\n",
        timeStampBuf, getpid(), GetTid(), g_logLevelMap[level].c_str(), fullTag.c_str(), logBuf);
#endif
}
