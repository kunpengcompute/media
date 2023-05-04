/*
 * 功能说明: 视频解码器功能接口实现
 */

#define LOG_TAG "VideoDecoderApi"
#include <utils/Log.h>
#include "VideoDecoder.h"
#include "VideoDecoderNetint.h"

using namespace MediaCore;

DecoderRetCode CreateVideoDecoder(VideoDecoder** decoder)
{
    auto ptr = std::make_unique<VideoDecoderNetint>();
    *decoder = dynamic_cast<VideoDecoder*>(ptr.release());

    if (*decoder == nullptr) {
        ALOGE("create video decoder failed: decoder type.");
        return VIDEO_DECODER_CREATE_FAIL;
    }
    return VIDEO_DECODER_SUCCESS;
}

DecoderRetCode DestroyVideoDecoder(VideoDecoder* decoder)
{
    if (decoder == nullptr) {
        ALOGW("input decoder is null.");
        return VIDEO_DECODER_SUCCESS;
    }

    VideoDecoderNetint *netint = nullptr;
    netint = static_cast<VideoDecoderNetint *>(decoder);
    delete netint;
    netint = nullptr;

    return VIDEO_DECODER_SUCCESS;
}
