/*
 * 功能说明: 适配OpenH264软件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */
#ifndef VIDEO_ENCODER_OPEN_H264_H
#define VIDEO_ENCODER_OPEN_H264_H

#include <string>
#include <atomic>
#include "VideoCodecApi.h"
#include "codec_api.h"

namespace OpenH264 {
    constexpr uint32_t DEFAULT_WIDTH = 720;
    constexpr uint32_t DEFAULT_HEIGHT = 1280;
    constexpr uint32_t FRAMERATE_MIN = 30;
    constexpr uint32_t FRAMERATE_MAX = 60;
    constexpr uint32_t GOPSIZE_MIN = 30;
    constexpr uint32_t GOPSIZE_MAX = 3000;
    constexpr uint32_t BITRATE_MIN = 1000000;
    constexpr uint32_t BITRATE_MAX = 10000000;
    constexpr uint32_t BITRATE_DEFAULT_264 = 5000000;
    const std::string ENCODE_PROFILE_BASELINE = "baseline";
    const std::string ENCODE_PROFILE_MAIN = "main";
    const std::string ENCODE_PROFILE_HIGH = "high";
}

class VideoEncoderOpenH264 : public VideoEncoder {
public:
    /**
     * @功能描述: 构造函数
     */
    VideoEncoderOpenH264();

    /**
     * @功能描述: 析构函数
     */
    ~VideoEncoderOpenH264() override;

    /**
     * @功能描述: 初始化编码器
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_INIT_FAIL 初始化编码器失败
     */
    EncoderRetCode InitEncoder() override;

    /**
     * @功能描述: 启动编码器
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_START_FAIL 启动编码器失败
     */
    EncoderRetCode StartEncoder() override;

    /**
     * @功能描述: 编码一帧数据
     * @参数 [in] inputData: 编码输入数据地址
     * @参数 [in] inputSize: 编码输入数据大小
     * @参数 [out] outputData: 编码输出数据地址
     * @参数 [out] outputSize: 编码输出数据大小
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_ENCODE_FAIL 编码一帧失败
     */
    EncoderRetCode EncodeOneFrame(const uint8_t *inputData, uint32_t inputSize,
        uint8_t **outputData, uint32_t *outputSize) override;

    /**
     * @功能描述: 停止编码器
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_STOP_FAIL 停止编码器失败
     */
    EncoderRetCode StopEncoder() override;

    /**
     * @功能描述: 销毁编码器，释放编码资源
     */
    void DestroyEncoder() override;

    /**
     * @功能描述: 重置编码器
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_RESET_FAIL 重置编码器失败
     */
    EncoderRetCode ResetEncoder() override;

    /**
     * @功能描述: 强制I帧
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_FORCE_KEY_FRAME_FAIL 强制I帧失败
     */
    EncoderRetCode ForceKeyFrame();

    /**
     * @功能描述: 设置编码参数
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_SET_ENCODE_PARAMS_FAIL 设置编码参数失败
     */
    EncoderRetCode SetEncodeParams();

    /**
     * @功能描述: 判断编码参数是否改变
     */
    bool EncodeParamsChange();

private:
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
     * @功能描述: 初始化编码器参数
     * @返回值: true 成功
     *          false 失败
     */
    bool InitParams();

    /**
     * @功能描述: 初始化编码器参数
     */
    void InitParamExt();

    /**
     * @功能描述: 初始化源数据
     * @参数 [in] inputData: 编码输入数据地址
     */
    void InitSrcPic(const uint8_t *inputData);

    /**
     * @功能描述: 资源释放
     */
    void Release();

    /**
     * @功能描述: 加载OpenH264动态库
     * @返回值: true 成功
     *          false 失败
     */
    bool LoadOpenH264SharedLib();

    EncodeParams m_encParams = {static_cast<uint32_t>(OpenH264::FRAMERATE_MIN),
        static_cast<uint32_t>(OpenH264::BITRATE_DEFAULT_264),
        static_cast<uint32_t>(OpenH264::GOPSIZE_MIN),
        OpenH264::ENCODE_PROFILE_BASELINE,
        static_cast<uint32_t>(OpenH264::DEFAULT_WIDTH),
        static_cast<uint32_t>(OpenH264::DEFAULT_HEIGHT)};
    EncodeParams m_tmpEncParams =  {static_cast<uint32_t>(OpenH264::FRAMERATE_MIN),
        static_cast<uint32_t>(OpenH264::BITRATE_DEFAULT_264),
        static_cast<uint32_t>(OpenH264::GOPSIZE_MIN),
        OpenH264::ENCODE_PROFILE_BASELINE,
        static_cast<uint32_t>(OpenH264::DEFAULT_WIDTH),
        static_cast<uint32_t>(OpenH264::DEFAULT_HEIGHT)};
    std::atomic<bool> m_resetFlag = { false };
    ISVCEncoder *m_encoder = nullptr;
    SEncParamExt m_paramExt = {};
    SSourcePicture m_srcPic = {};
    SFrameBSInfo m_frameBSInfo = {};
    uint32_t m_yLength = 0;
    uint32_t m_frameSize = 0;
};

#endif  // VIDEO_ENCODER_OPEN_H264_H
