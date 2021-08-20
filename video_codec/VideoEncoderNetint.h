/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 适配NETINT硬件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */
#ifndef VIDEO_ENCODER_NETINT_H
#define VIDEO_ENCODER_NETINT_H

#include <string>
#include <unordered_map>
#include <atomic>
#include "VideoCodecApi.h"
#include "ni_device_api.h"
#include "ni_defs.h"
#include "ni_rsrc_api.h"

enum NiCodecType : uint32_t {
    NI_CODEC_TYPE_H264 = 0,
    NI_CODEC_TYPE_H265 = 1
};

namespace {
    constexpr uint32_t DEFAULT_WIDTH = 720;
    constexpr uint32_t DEFAULT_HEIGHT = 1280;
    const std::string NI_ENCODER_INIT_DEFAULT_PARAMS = "ni_encoder_init_default_params";
    const std::string NI_ENCODER_PARAMS_SET_VALUE = "ni_encoder_params_set_value";
    const std::string NI_RSRC_ALLOCATE_AUTO = "ni_rsrc_allocate_auto";
    const std::string NI_RSRC_RELEASE_RESOURCE = "ni_rsrc_release_resource";
    const std::string NI_RSRC_FREE_DEVICE_CONTEXT = "ni_rsrc_free_device_context";
    const std::string NI_DEVICE_OPEN = "ni_device_open";
    const std::string NI_DEVICE_SESSION_CONTEXT_INIT = "ni_device_session_context_init";
    const std::string NI_DEVICE_SESSION_OPEN = "ni_device_session_open";
    const std::string NI_DEVICE_SESSION_WRITE = "ni_device_session_write";
    const std::string NI_DEVICE_SESSION_READ = "ni_device_session_read";
    const std::string NI_DEVICE_SESSION_CLOSE = "ni_device_session_close";
    const std::string NI_FRAME_BUFFER_ALLOC_V3 = "ni_frame_buffer_alloc_v3";
    const std::string NI_FRAME_BUFFER_FREE = "ni_frame_buffer_free";
    const std::string NI_PACKET_BUFFER_ALLOC = "ni_packet_buffer_alloc";
    const std::string NI_PACKET_BUFFER_FREE = "ni_packet_buffer_free";
    const std::string NI_GET_HW_YUV420P_DIM = "ni_get_hw_yuv420p_dim";
    const std::string NI_COPY_HW_YUV420P = "ni_copy_hw_yuv420p";

    using NiEncInitDefaultParamsFunc =
        ni_retcode_t (*)(ni_encoder_params_t *param, int fpsNum, int fpsDenom, long bitRate, int width, int height);
    using NiEncParamsSetValueFunc =
        ni_retcode_t (*)(ni_encoder_params_t *params, const char *name, const char *value);
    using NiRsrcAllocateAutoFunc = ni_device_context_t* (*)(ni_device_type_t devType, ni_alloc_rule_t rule,
        ni_codec_t codec, int width, int height, int frameRate, unsigned long *load);
    using NiRsrcReleaseResourceFunc = void (*)(ni_device_context_t *devCtx, ni_codec_t codec, unsigned long load);
    using NiRsrcFreeDeviceContextFunc = void (*)(ni_device_context_t *devCtx);
    using NiDeviceOpenFunc = ni_device_handle_t (*)(const char *dev, uint32_t *maxIoSizeOut);
    using NiDeviceSessionContextInitFunc = void (*)(ni_session_context_t *sessionCtx);
    using NiDeviceSessionOpenFunc = ni_retcode_t (*)(ni_session_context_t *sessionCtx, ni_device_type_t devType);
    using NiDeviceSessionWriteFunc =
        int (*)(ni_session_context_t *sessionCtx, ni_session_data_io_t *sessionDataIo, ni_device_type_t devType);
    using NiDeviceSessionReadFunc =
        int (*)(ni_session_context_t *sessionCtx, ni_session_data_io_t *sessionDataIo, ni_device_type_t devType);
    using NiDeviceSessionCloseFunc =
        ni_retcode_t (*)(ni_session_context_t *sessionCtx, int eosRecieved, ni_device_type_t devType);
    using NiFrameBufferAllocV3Func = ni_retcode_t (*)(ni_frame_t *frame, int videoWidth,
        int videoHeight, int lineSize[], int alignment, int extraLen);
    using NiFrameBufferFreeFunc = ni_retcode_t (*)(ni_frame_t *frame);
    using NiPacketBufferAllocFunc = ni_retcode_t (*)(ni_packet_t *packet, int packetSize);
    using NiPacketBufferFreeFunc = ni_retcode_t (*)(ni_packet_t *packet);
    using NiGetHwYuv420pDimFunc = void (*)(int width, int height, int bitDepthFactor, int isH264,
        int planeStride[NI_MAX_NUM_DATA_POINTERS], int planeHeight[NI_MAX_NUM_DATA_POINTERS]);
    using NiCopyHwYuv420pFunc = void (*)(uint8_t *dstPtr[NI_MAX_NUM_DATA_POINTERS],
        uint8_t *srcPtr[NI_MAX_NUM_DATA_POINTERS], int frameWidth, int frameHeight, int bitDepthFactor,
        int dstStride[NI_MAX_NUM_DATA_POINTERS], int dstHeight[NI_MAX_NUM_DATA_POINTERS],
        int srcStride[NI_MAX_NUM_DATA_POINTERS],int srcHeight[NI_MAX_NUM_DATA_POINTERS]);
}

class VideoEncoderNetint : public VideoEncoder {
public:
    /**
     * @功能描述: 构造函数
     */
    explicit VideoEncoderNetint(NiCodecType codecType);

    /**
     * @功能描述: 析构函数
     */
    ~VideoEncoderNetint() override;

    /**
     * @功能描述: 初始化编码器
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_INIT_FAIL 初始化编码器失败
     */
    EncoderRetCode InitEncoder(const EncodeParams &encParams) override;

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
    EncoderRetCode ForceKeyFrame() override;

    /**
     * @功能描述: 设置编码参数
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: VIDEO_ENCODER_SUCCESS 成功
     *          VIDEO_ENCODER_SET_ENCODE_PARAMS_FAIL 设置编码参数失败
     */
    EncoderRetCode SetEncodeParams(const EncodeParams &encParams) override;

private:
    /**
     * @功能描述: 校验编码参数合法性
     * @参数 [in] encParams: 编码参数结构体
     * @返回值: true 成功
     *          false 失败
     */
    bool VerifyEncodeParams(const EncodeParams &encParams);

    /**
     * @功能描述: 加载NETINT动态库
     * @返回值: true 成功
     *          false 失败
     */
    bool LoadNetintSharedLib();

    /**
     * @功能描述: 卸载NETINT动态库
     */
    void UnloadNetintSharedLib();

    /**
     * @功能描述: 初始化编码器资源
     * @返回值: true 成功
     *          false 失败
     */
    bool InitCodec();

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

    ni_codec_t m_codec = EN_H264;
    EncodeParams m_encParams = {};
    std::atomic<bool> m_resetFlag = { false };
    ni_encoder_params_t m_niEncParams = {};
    ni_session_context_t m_sessionCtx = {};
    ni_device_context_t *m_devCtx = nullptr;
    ni_session_data_io_t m_frame = {};
    ni_session_data_io_t m_packet = {};
    int m_width = DEFAULT_WIDTH;
    int m_height = DEFAULT_HEIGHT;
    int m_widthAlign = DEFAULT_WIDTH;
    int m_heightAlign = DEFAULT_HEIGHT;
    unsigned long m_load = 0;
    void *m_libHandle = nullptr;
    std::unordered_map<std::string, void*> m_funcMap = {
        { NI_ENCODER_INIT_DEFAULT_PARAMS, nullptr },
        { NI_ENCODER_INIT_DEFAULT_PARAMS, nullptr },
        { NI_ENCODER_PARAMS_SET_VALUE, nullptr },
        { NI_RSRC_ALLOCATE_AUTO, nullptr },
        { NI_RSRC_RELEASE_RESOURCE, nullptr },
        { NI_RSRC_FREE_DEVICE_CONTEXT, nullptr },
        { NI_DEVICE_OPEN, nullptr },
        { NI_DEVICE_SESSION_CONTEXT_INIT, nullptr },
        { NI_DEVICE_SESSION_OPEN, nullptr },
        { NI_DEVICE_SESSION_WRITE, nullptr },
        { NI_DEVICE_SESSION_READ, nullptr },
        { NI_DEVICE_SESSION_CLOSE, nullptr },
        { NI_FRAME_BUFFER_ALLOC_V3, nullptr },
        { NI_FRAME_BUFFER_FREE, nullptr },
        { NI_PACKET_BUFFER_ALLOC, nullptr },
        { NI_PACKET_BUFFER_FREE, nullptr },
        { NI_GET_HW_YUV420P_DIM, nullptr },
        { NI_COPY_HW_YUV420P, nullptr }
    };
};

#endif  // VIDEO_ENCODER_NETINT_H
