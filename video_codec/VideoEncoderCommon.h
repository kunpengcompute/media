/*
 * 功能说明: 编码参数公共部分代码
 */
#ifndef VIDEO_ENCODER_COMMON_H
#define VIDEO_ENCODER_COMMON_H

#include <string>
#include <unordered_map>
#include <atomic>
#include "VideoCodecApi.h"

namespace {
    constexpr int32_t PORTRAIT_WIDTH_MAX = 4320;
    constexpr int32_t PORTRAIT_WIDTH_MIN = 144;
    constexpr int32_t PORTRAIT_HEIGHT_MAX = 7680;
    constexpr int32_t PORTRAIT_HEIGHT_MIN = 176;
    constexpr int32_t LANDSCAPE_WIDTH_MAX = 7680;
    constexpr int32_t LANDSCAPE_WIDTH_MIN = 176;
    constexpr int32_t LANDSCAPE_HEIGHT_MAX = 4320;
    constexpr int32_t LANDSCAPE_HEIGHT_MIN = 144;
    constexpr uint32_t DEFAULT_WIDTH = 720;
    constexpr uint32_t DEFAULT_HEIGHT = 1280;
    constexpr uint32_t FRAMERATE_MIN = 30;
    constexpr uint32_t FRAMERATE_MAX = 60;
    constexpr uint32_t GOPSIZE_MIN = 30;
    constexpr uint32_t GOPSIZE_MAX = 3000;
    constexpr uint32_t BITRATE_MIN = 1000000;
    constexpr uint32_t BITRATE_MAX = 10000000;
    constexpr uint32_t BITRATE_DEFAULT_264 = 5000000;
    constexpr uint32_t BITRATE_DEFAULT_265 = 3000000;
    const std::string ENCODE_PROFILE_BASELINE = "baseline";
    const std::string ENCODE_PROFILE_MAIN = "main";
    const std::string ENCODE_PROFILE_HIGH = "high";
    enum EncoderType : uint32_t {
    ENCODER_TYPE_OPENH264 = 0,    // 开源OpenH264编码器
    ENCODER_TYPE_NETINTH264 = 1,  // NETINT h.264硬件编码器
    ENCODER_TYPE_NETINTH265 = 2,   // NETINT h.265硬件编码器
    ENCODER_TYPE_VASTAIH264 = 3,  // VASTAI h.264硬件编码器
    ENCODER_TYPE_VASTAIH265 = 4,  // VASTAI h.265硬件编码器
    ENCODER_TYPE_QUATRAH264 = 5,  // QUATRA h.264硬件编码器
    ENCODER_TYPE_QUADRAH265 = 6,  // QUATRA h.265硬件编码器
};
}

class VideoEncoderCommon {
public:
    /**
     * @功能描述: 默认构造函数
     */
    VideoEncoderCommon() = default;

    /**
     * @功能描述: 默认析构函数
     */
    virtual ~VideoEncoderCommon() = default;
    
    /**
     * @功能描述: 设置编码参数
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_SET_ENCODE_PARAMS_FAIL 设置编码参数失败
     */
    EncoderRetCode SetEncodeParams();

    /**
     * @功能描述: 判断编码参数是否改变
     */
    bool EncodeParamsChange();

        /**
     * @功能描述: 获取ro编码参数
     * @返回值: true 成功
     *          false 失败
     */
    bool GetRoEncParam();

    /**
     * @功能描述: 获取Persist编码参数
     * @返回值: true 成功
     *          false 失败
     */
    bool GetPersistEncParam();

    /**
     * @功能描述: 校验编码参数合法性
     * @参数 [in] width 屏幕宽,height 屏幕高,framerate 帧率
     * @返回值: true 成功
     *          false 失败
     */
    bool VerifyEncodeRoParams(int32_t width, int32_t height, int32_t framerate);

    /**
     * @功能描述: 校验编码参数合法性
     * @参数 [in] bitrate 码率,gopsize gop间隔,profile 配置
     * @返回值: true 成功
     *          false 失败
     */
    bool VerifyEncodeParams(std::string &bitrate, std::string &gopsize, std::string &profile);

    /**
     * @功能描述: 单帧编码参数校验
     */
    EncoderRetCode EncodeParamsCheck();

protected:
    // 编码参数
    struct EncodeParams {
        uint32_t framerate = 0;
        uint32_t bitrate = 0;    // 编码输出码率
        uint32_t gopsize = 0;    // 关键帧间隔
        std::string profile = "";    // 编码档位
        uint32_t width = 0;      // 编码输入/输出宽度
        uint32_t height = 0;     // 编码输入/输出高度

        bool operator==(const EncodeParams &rhs) const
        {
            return (framerate == rhs.framerate) && (bitrate == rhs.bitrate) && (gopsize == rhs.gopsize) &&
                (profile == rhs.profile) && (width == rhs.width) && (height == rhs.height);
        }
    };

    EncodeParams m_encParams = {static_cast<uint32_t>(FRAMERATE_MIN), static_cast<uint32_t>(BITRATE_DEFAULT_264),
        static_cast<uint32_t>(GOPSIZE_MIN), ENCODE_PROFILE_BASELINE, static_cast<uint32_t>(DEFAULT_WIDTH),
        static_cast<uint32_t>(DEFAULT_HEIGHT)};
    EncodeParams m_tmpEncParams = {static_cast<uint32_t>(FRAMERATE_MIN), static_cast<uint32_t>(BITRATE_DEFAULT_264),
        static_cast<uint32_t>(GOPSIZE_MIN), ENCODE_PROFILE_BASELINE, static_cast<uint32_t>(DEFAULT_WIDTH),
        static_cast<uint32_t>(DEFAULT_HEIGHT)};
    std::atomic<bool> m_resetFlag = { false };
};

#endif  // VIDEO_ENCODER_COMMON_H
