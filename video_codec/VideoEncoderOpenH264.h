/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 适配OpenH264软件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */
#ifndef VIDEO_ENCODER_OPEN_H264_H
#define VIDEO_ENCODER_OPEN_H264_H

#include "VideoCodecApi.h"
#include "codec_api.h"

namespace {
    /**
     * @功能描述: 创建编码器实例
     * @参数 [out] encoder: 编码器实例
     * @返回值: 0为成功；其他为失败
     */
    using WelsCreateSVCEncoderFuncPtr = int (*)(ISVCEncoder** encoder);

    /**
     * @功能描述: 销毁编码器实例
     * @参数 [in] encoder: 编码器实例
     */
    using WelsDestroySVCEncoderFuncPtr = int (*)(ISVCEncoder* encoder);
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
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_INIT_FAIL 初始化编码器失败
     */
    EncoderRetCode InitEncoder(const EncoderParams &encParams) override;

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

private:
    /**
     * @功能描述: 校验编码参数合法性
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: true 成功
     *          false 失败
     */
    bool VerifyEncodeParam(const EncoderParams &encParams);

    /**
     * @功能描述: 初始化编码器参数
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: true 成功
     *          false 失败
     */
    bool InitParams(const EncoderParams &encParams);

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

    /**
     * @功能描述: 卸载OpenH264动态库
     */
    void UnloadOpenH264SharedLib();

    ISVCEncoder *m_encoder = nullptr;
    SEncParamExt m_paramExt = {};
    SSourcePicture m_srcPic = {};
    SFrameBSInfo m_frameBSInfo = {};
    uint32_t m_yLength = 0;
    uint32_t m_frameSize = 0;
    WelsCreateSVCEncoderFuncPtr m_welsCreateSVCEncoder = nullptr;
    WelsDestroySVCEncoderFuncPtr m_welsDestroySVCEncoder = nullptr;
    void *m_libHandle = nullptr;
};

#endif  // VIDEO_ENCODER_OPEN_H264_H
