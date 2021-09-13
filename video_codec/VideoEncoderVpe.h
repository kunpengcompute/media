/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 适配涌现科技硬件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */
#ifndef VIDEO_ENCODER_VPE_H
#define VIDEO_ENCODER_VPE_H

#include <vector>
#include <memory>
#include <atomic>

#include "VideoCodecApi.h"
#include "vpi_api.h"
#include "vpi_types.h"

namespace {
    constexpr uint32_t WIDTH_DEFAULT = 720;
    constexpr uint32_t HEIGHT_DEFAULT = 1280;
}

struct VpeDevCtx {
    int device;
    VpiApi *func;
    bool initialized;
};

struct VpeCodecCtx {
    VpiCtx ctx;
    VpiApi *vpi;
    VpiEncParamSet *paramList;
    VpiH26xEncCfg *config;
    bool initialized;
};

struct VpePPCtx {
    VpiCtx ctx;
    VpiApi *vpi;
    VpiPPOption *option;
    bool initialized;
};

struct VpeContext {
    VpeDevCtx devCtx;
    VpiSysInfo *sysInfo;
    VpeCodecCtx encCtx;
    VpePPCtx ppCtx;
    VpiFrame *frameCtx;
};

class VideoEncoderVpe : public VideoEncoder {
public:
    /**
     * @功能描述: 构造函数
     */
    explicit VideoEncoderVpe(VpiH26xCodecID id);

    /**
     * @功能描述: 析构函数
     */
    ~VideoEncoderVpe() override;

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
    bool LoadVpeSharedLib();

    bool VerifyEncodeParams(const EncodeParams &encParams);

    bool InitFrameData(const uint8_t *src);

    bool CreateVpeDevice();

    void DestoryVpeDevice();

    bool InitVpeFilter();

    void CloseVpeFilter();

    bool InitVpeCodec();

    void FlushVpeCodec();

    void CloseVpeCodec();

    bool CreateVpeParamsList();

    void ReleaseVpeParamsList();


    EncodeParams m_encParams {};
    std::atomic<bool> m_resetFlag {false};
    VpiH26xCodecID m_codec {CODEC_ID_H264};
    VpeContext m_vpeContext {};
    VpiFrame m_frame {};
    VpiPacket m_packet {};
    VpiFrame *m_filterFrame {nullptr};
    std::vector<uint8_t> m_streamBuffer {};
    std::unique_ptr<VpiFrame> m_frameCtx {nullptr};
    uint32_t m_frameCount {0};
    uint32_t m_width {WIDTH_DEFAULT};
    uint32_t m_height {HEIGHT_DEFAULT};

};

namespace VpeSrm {
    struct  SrmDriverStatus {
        uint32_t devId;
        int powerState;
        int encUsage;
        int usedMem;
        int freeMem;
    };

    struct SrmContext {
        SrmDriverStatus *driverStatus;
        uint32_t driverNums;
    };

    uint32_t GetDeviceNumbers();

    bool GetPowerState(uint32_t devId, SrmDriverStatus &status);

    bool GetEncUsage(uint32_t devId, SrmDriverStatus &status);

    bool GetMemUsage(uint32_t devId, SrmDriverStatus &status);

    bool ReadDriverStatus(SrmContext &srm);

    int SrmAllocateResource();
}

#endif  // VIDEO_ENCODER_VPE_H
