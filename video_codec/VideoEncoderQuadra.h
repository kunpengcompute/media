/*
 * 版权所有 (c) 华为技术有限公司 2023-2023
 * 功能说明: 适配Quadra硬件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */
#ifndef VIDEO_ENCODER_QUADRA_H
#define VIDEO_ENCODER_QUADRA_H

#include <string>
#include <unordered_map>
#include <atomic>
#include <libavutil/hwcontext.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "VideoCodecApi.h"
#include "VideoEncoderCommon.h"

enum QuaCodecType : uint32_t {
    QUA_CODEC_TYPE_H264 = 0,
    QUA_CODEC_TYPE_H265 = 1
};

class VideoEncoderQuadra : public VideoEncoder, public VideoEncoderCommon {
public:
    /**
     * @功能描述: 构造函数
     */
    explicit VideoEncoderQuadra(QuaCodecType codecType);

    /**
     * @功能描述: 析构函数
     */
    ~VideoEncoderQuadra() override;

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
     * @功能描述: 加载Quadra动态库
     * @返回值: true 成功
     *          false 失败
     */
    bool LoadQuadraSharedLib();

    /**
     * @功能描述: 卸载Quadra动态库
     */
    void UnLoadQuadraSharedLib();

    /**
     * @功能描述: 检查map中函数指针是否存在空指针
     */
    void CheckFuncPtr();

    /**
     * @功能描述: 初始化编码器上下文参数
     * @返回值: true 成功
     *          false 失败
     */
    bool InitCtxParams();

    /**
     * @功能描述: 拷贝一帧数据到编码器
     * @参数 [in] src: 待编码数据地址
     * @返回值: true 成功
     *          false 失败
     */
    bool InitFrameData(const uint8_t *src);

    /**
     * @功能描述: 接收一帧已编码的码流数据
     * @参数 [out] outputData: 编码输出数据地址
     * @参数 [out] outputSize: 编码输出数据大小
     * @返回值: true 成功
     *          false 接收一帧失败
     */
    bool ReceiveOneFrame(uint8_t **outputData, uint32_t *outputSize);

    int m_width = DEFAULT_WIDTH;
    int m_height = DEFAULT_HEIGHT;
    bool m_funPtrError = false;
    bool m_isInited = false;
    std::string m_codec = "h264_ni_quadra_enc";
    AVCodec *m_encCodec = nullptr;
    AVCodecContext *m_encoderCtx = nullptr;
    AVPacket *m_encPkt = nullptr;
    AVFrame *m_swFrame = nullptr;
};

#endif  // VIDEO_ENCODER_Quadra_H