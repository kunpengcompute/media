/*
 * 功能说明: 视频编解码器对外接口实现
 */

#define LOG_TAG "VideoCodecApi"
#include "VideoCodecApi.h"
#include "VideoEncoderNetint.h"
#include "VideoEncoderOpenH264.h"
#include "MediaLog.h"
#include "Property.h"

namespace {
// 编码器类型
enum EncoderType : uint32_t {
    ENCODER_TYPE_OPENH264 = 0,    // 开源OpenH264编码器
    ENCODER_TYPE_NETINTH264 = 1,  // NETINT h.264硬件编码器
    ENCODER_TYPE_NETINTH265 = 2   // NETINT h.265硬件编码器
};
}

EncoderRetCode CreateVideoEncoder(VideoEncoder **encoder)
{
    uint32_t encType = GetIntEncParam("ro.vmi.demo.video.encode.format");
    INFO("create video encoder: encoder type %u", encType);
    switch (encType) {
        case ENCODER_TYPE_OPENH264:
            *encoder = new (std::nothrow) VideoEncoderOpenH264();
            break;
        case ENCODER_TYPE_NETINTH264:
            *encoder = new (std::nothrow) VideoEncoderNetint(NI_CODEC_TYPE_H264);
            break;
        case ENCODER_TYPE_NETINTH265:
            *encoder = new (std::nothrow) VideoEncoderNetint(NI_CODEC_TYPE_H265);
            break;
        default:
            ERR("create video encoder failed: unknown encoder type %u", encType);
            return VIDEO_ENCODER_CREATE_FAIL;
    }
    if (*encoder == nullptr) {
        ERR("create video encoder failed: encoder type %u", encType);
        return VIDEO_ENCODER_CREATE_FAIL;
    }
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode DestroyVideoEncoder(VideoEncoder *encoder)
{
    if (encoder == nullptr) {
        WARN("input encoder is null");
        return VIDEO_ENCODER_SUCCESS;
    }
    delete encoder;
    encoder = nullptr;
    return VIDEO_ENCODER_SUCCESS;
}
