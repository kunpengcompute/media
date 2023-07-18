/*
 * 功能说明: 编码参数公共部分代码
 */
#define LOG_TAG "VideoEncoderCommon"
#include "VideoEncoderCommon.h"
#include <string>
#include <unordered_map>
#include <atomic>
#include "MediaLog.h"
#include "Property.h"

/**
    * @功能描述: 设置编码参数
    * @返回值: VIDEO_ENCODER_SUCCESS 成功
    *          VIDEO_ENCODER_SET_ENCODE_PARAMS_FAIL 设置编码参数失败
    */
EncoderRetCode VideoEncoderCommon::SetEncodeParams()
{
    if (EncodeParamsChange()) {
        m_encParams = m_tmpEncParams;
        m_resetFlag = true;
        INFO("Handle encoder config change: [bitrate, gopsize, profile] = [%u,%u,%s]",
            m_encParams.bitrate, m_encParams.gopsize, m_encParams.profile.c_str());
    } else {
        INFO("Using encoder config: [bitrate, gopsize, profile] = [%u,%u,%s]",
            m_encParams.bitrate, m_encParams.gopsize, m_encParams.profile.c_str());
    }
    return VIDEO_ENCODER_SUCCESS;
}

/**
    * @功能描述: 判断编码参数是否改变
    */
bool VideoEncoderCommon::EncodeParamsChange()
{
    return (m_tmpEncParams.bitrate != m_encParams.bitrate) || (m_tmpEncParams.gopsize != m_encParams.gopsize) ||
        (m_tmpEncParams.profile != m_encParams.profile) || (m_tmpEncParams.width != m_encParams.width) ||
        (m_tmpEncParams.height != m_encParams.height) || (m_tmpEncParams.framerate != m_encParams.framerate);
}

/**
    * @功能描述: 获取ro编码参数
    * @返回值: true 成功
    *          false 失败
    */
bool VideoEncoderCommon::GetRoEncParam()
{
    int32_t width = 0;
    int32_t height = 0;
    int32_t framerate = 0;
    std::string phoneMode = GetStrEncParam("ro.sys.vmi.cloudphone");
    if (phoneMode == "video") {
        width = GetIntEncParam("ro.hardware.width");
        height = GetIntEncParam("ro.hardware.height");
        framerate = GetIntEncParam("ro.hardware.fps");
    } else if (phoneMode == "instruction") {
        width = GetIntEncParam("persist.vmi.demo.video.encode.width");
        height = GetIntEncParam("persist.vmi.demo.video.encode.height");
        framerate = GetIntEncParam("persist.vmi.demo.video.encode.framerate");
    } else {
        ERR("Invalid property value[%s] for property[ro.sys.vmi.cloudphone], get property failed!", phoneMode.c_str());
        return false;
    }

    if (!VerifyEncodeRoParams(width, height, framerate)) {
        ERR("encoder params is not supported");
        return false;
    }

    m_tmpEncParams.width = width;
    m_tmpEncParams.height = height;
    m_tmpEncParams.framerate = framerate;
    return true;
}

/**
    * @功能描述: 获取Persist编码参数
    * @返回值: true 成功
    *          false 失败
    */
bool VideoEncoderCommon::GetPersistEncParam()
{
    std::string bitrate = "";
    std::string gopsize = "";
    std::string profile = "";
    std::string phoneMode = GetStrEncParam("ro.sys.vmi.cloudphone");
    if (phoneMode == "video") {
        bitrate = GetStrEncParam("persist.vmi.video.encode.bitrate");
        gopsize = GetStrEncParam("persist.vmi.video.encode.gopsize");
        profile = GetStrEncParam("persist.vmi.video.encode.profile");
    } else if (phoneMode == "instruction") {
        bitrate = GetStrEncParam("persist.vmi.demo.video.encode.bitrate");
        gopsize = GetStrEncParam("persist.vmi.demo.video.encode.gopsize");
        profile = GetStrEncParam("persist.vmi.demo.video.encode.profile");
    } else {
        ERR("Invalid property value[%s] for property[ro.sys.vmi.cloudphone], get property failed!", phoneMode.c_str());
        return false;
    }

    if (!VerifyEncodeParams(bitrate, gopsize, profile)) {
        SetEncParam("persist.vmi.video.encode.bitrate", std::to_string(m_encParams.bitrate).c_str());
        SetEncParam("persist.vmi.video.encode.gopsize", std::to_string(m_encParams.gopsize).c_str());
        SetEncParam("persist.vmi.video.encode.profile", m_encParams.profile.c_str());
    } else {
        m_tmpEncParams.bitrate = StrToInt(bitrate);
        m_tmpEncParams.gopsize = StrToInt(gopsize);
        m_tmpEncParams.profile = profile;
    }

    return true;
}

/**
    * @功能描述: 校验编码参数合法性
    * @参数 [in] width 屏幕宽,height 屏幕高,framerate 帧率
    * @返回值: true 成功
    *          false 失败
    */
bool VideoEncoderCommon::VerifyEncodeRoParams(int32_t width, int32_t height, int32_t framerate)
{
    bool isEncodeParamsTrue = true;

    if (width > height) {
        if ((width > LANDSCAPE_WIDTH_MAX) || (height > LANDSCAPE_HEIGHT_MAX) ||
            (width < LANDSCAPE_WIDTH_MIN) || (height < LANDSCAPE_HEIGHT_MIN)) {
            ERR("Invalid property value[%dx%d] for property[width,height], get property failed!", width, height);
            isEncodeParamsTrue = false;
        }
    } else {
        if ((width > PORTRAIT_WIDTH_MAX) || (height > PORTRAIT_HEIGHT_MAX) ||
            (width < PORTRAIT_WIDTH_MIN) || (height < PORTRAIT_HEIGHT_MIN)) {
            ERR("Invalid property value[%dx%d] for property[width,height], get property failed!", width, height);
            isEncodeParamsTrue = false;
        }
    }

    if ((framerate != FRAMERATE_MIN) && (framerate != FRAMERATE_MAX)) {
        ERR("Invalid property value[%d] for property[framerate], get property failed!", framerate);
        isEncodeParamsTrue = false;
    }
    return isEncodeParamsTrue;
}

/**
    * @功能描述: 校验编码参数合法性
    * @参数 [in] bitrate 码率,gopsize gop间隔,profile 配置
    * @返回值: true 成功
    *          false 失败
    */
bool VideoEncoderCommon::VerifyEncodeParams(std::string &bitrate, std::string &gopsize, std::string &profile)
{
    bool isEncodeParamsTrue = true;
    if ((StrToInt(bitrate) < BITRATE_MIN) || (StrToInt(bitrate) > BITRATE_MAX)) {
        WARN("Invalid property value[%s] for property[bitrate], use last correct encode bitrate[%u]",
            bitrate.c_str(), m_encParams.bitrate);
        isEncodeParamsTrue = false;
    }

    if ((StrToInt(gopsize) < GOPSIZE_MIN) || (StrToInt(gopsize) > GOPSIZE_MAX)) {
    WARN("Invalid property value[%s] for property[gopsize], use last correct encode gopsize[%u]",
        gopsize.c_str(), m_encParams.gopsize);
    isEncodeParamsTrue = false;
    }
    
    uint32_t encType = GetIntEncParam("ro.vmi.demo.video.encode.format");
    if ((encType == ENCODER_TYPE_OPENH264) || (encType == ENCODER_TYPE_NETINTH264) ||
        (encType == ENCODER_TYPE_VASTAIH264)) {
        if (profile != ENCODE_PROFILE_BASELINE &&
            profile != ENCODE_PROFILE_MAIN &&
            profile != ENCODE_PROFILE_HIGH) {
            WARN("Invalid property value[%s] for property[profile], use last correct encode profile[%s]",
                profile.c_str(), m_encParams.profile.c_str());
            isEncodeParamsTrue = false;
            }
    } else if ((encType == ENCODER_TYPE_NETINTH265) || (encType == ENCODER_TYPE_VASTAIH265)) {
                if (profile != ENCODE_PROFILE_MAIN) {
                    WARN("Invalid property value[%s] for property[profile], use last correct encode profile[%s]",
                        profile.c_str(), m_encParams.profile.c_str());
                    isEncodeParamsTrue = false;
                }
        }
    
    return isEncodeParamsTrue;
}

/**
* @功能描述: 单帧编码参数校验
*/
EncoderRetCode VideoEncoderCommon::EncodeParamsCheck()
{
    std::string isParamChange = GetStrEncParam( "persist.vmi.video.encode.param_adjusting");
    if (isParamChange == "1") {
        if ((!GetRoEncParam()) || (!GetPersistEncParam())) {
            ERR("init encoder failed: GetEncParam failed");
            return VIDEO_ENCODER_INIT_FAIL;
        }
        SetEncodeParams();
        SetEncParam("persist.vmi.video.encode.param_adjusting", "0");
    } else if (isParamChange != "0") {
        WARN("Invalid property value[%s] for encode param adjusting", isParamChange.c_str());
        SetEncParam("persist.vmi.video.encode.param_adjusting", "0");
    }
    return VIDEO_ENCODER_SUCCESS;
}