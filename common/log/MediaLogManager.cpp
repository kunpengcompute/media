/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 媒体日志管理模块
 */

#include "MediaLogManager.h"

MediaLogManager& MediaLogManager::GetInstance()
{
    static MediaLogManager logManager;
    return logManager;
}

void MediaLogManager::SetLogCallback(MediaLogCallbackFunc logCallback)
{
    if (logCallback != nullptr) {
        m_logCallback = logCallback;
        m_logLevel = LOG_LEVEL_DEBUG;
    }
}

bool MediaLogManager::IsLogCallbackNull() const
{
    return m_logCallback == nullptr;
}

void MediaLogManager::Callback(int level, const std::string &tag, const std::string &logData) const
{
    if (m_logCallback != nullptr) {
        m_logCallback(level, tag.c_str(), logData.c_str());
    }
}

MediaLogLevel MediaLogManager::GetLogLevel() const
{
    return m_logLevel;
}
