/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 适配NETINT硬件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */

#define LOG_TAG "VideoEncoderNetint"
#include "VideoEncoderNetint.h"
#include <dlfcn.h>
#include <algorithm>
#include <cstring>
#include "MediaLog.h"

namespace {
    constexpr uint32_t FRAMERATE_MIN = 30;
    constexpr uint32_t FRAMERATE_MAX = 60;
    constexpr int GOP_SIZE_DEFAULT = 300;
    constexpr int Y_INDEX = 0;
    constexpr int U_INDEX = 1;
    constexpr int V_INDEX = 2;
    constexpr int BIT_DEPTH = 8;
    constexpr int NUM_OF_PLANES = 3;
    constexpr int COMPRESS_RATIO = 2;
    const std::string SHARED_LIB_NAME = "libxcoder.so";
}

VideoEncoderNetint::VideoEncoderNetint(NiCodecType codecType)
{
    if (codecType == NI_CODEC_TYPE_H264) {
        m_codec = EN_H264;
    } else {
        m_codec = EN_H265;
    }
    INFO("VideoEncoderNetint constructed %s", (m_codec == EN_H264) ? "h.264" : "h.265");
}

VideoEncoderNetint::~VideoEncoderNetint()
{
    DestroyEncoder();
    INFO("VideoEncoderNetint destructed");
}

EncoderRetCode VideoEncoderNetint::InitEncoder(const EncodeParams &encParams)
{
    if (!VerifyEncodeParams(encParams)) {
        ERR("init encoder failed: encoder params is not supported");
        return VIDEO_ENCODER_INIT_FAIL;
    }
    m_encParams = encParams;
    if (!LoadNetintSharedLib()) {
        ERR("init encoder failed: load NETINT so error");
        return VIDEO_ENCODER_INIT_FAIL;
    }
    m_width = static_cast<int>(m_encParams.width);
    m_height = static_cast<int>(m_encParams.height);
    const int align = (m_codec == EN_H264) ? 16 : 8;  // h.264: 16-aligned, h.265: 8-aligned
    m_widthAlign = std::max(((m_width + align - 1) / align) * align, NI_MIN_WIDTH);
    m_heightAlign = std::max(((m_height + align - 1) / align) * align, NI_MIN_HEIGHT);
    if (!InitCodec()) {
        ERR("init encoder failed: init codec error");
        return VIDEO_ENCODER_INIT_FAIL;
    }
    auto deviceSessionOpen = reinterpret_cast<NiDeviceSessionOpenFunc>(m_funcMap[NI_DEVICE_SESSION_OPEN]);
    ni_retcode_t ret = (*deviceSessionOpen)(&m_sessionCtx, NI_DEVICE_TYPE_ENCODER);
    if (ret != NI_RETCODE_SUCCESS) {
        ERR("init encoder failed: device session open error %d", ret);
        return VIDEO_ENCODER_INIT_FAIL;
    }
    m_frame.data.frame.start_of_stream = 1;
    INFO("init encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

bool VideoEncoderNetint::VerifyEncodeParams(const EncodeParams &encParams)
{
    if (encParams.width > NI_PARAM_MAX_WIDTH || encParams.height > NI_PARAM_MAX_HEIGHT ||
        encParams.width < NI_PARAM_MIN_WIDTH || encParams.height < NI_PARAM_MIN_HEIGHT) {
        ERR("resolution [%ux%u] is not supported", encParams.width, encParams.height);
        return false;
    }
    if (encParams.frameRate != FRAMERATE_MIN && encParams.frameRate != FRAMERATE_MAX) {
        ERR("framerate [%u] is not supported", encParams.frameRate);
        return false;
    }
    if (encParams.bitrate < NI_MIN_BITRATE && encParams.bitrate > NI_MAX_BITRATE) {
        ERR("bitrate [%u] is not supported", encParams.bitrate);
        return false;
    }
    INFO("width:%u, height:%u, framerate:%u, bitrate:%u, gopsize:%u, profile:%u", encParams.width, encParams.height,
        encParams.frameRate, encParams.bitrate, encParams.gopSize, encParams.profile);
    return true;
}

bool VideoEncoderNetint::LoadNetintSharedLib()
{
    INFO("load %s", SHARED_LIB_NAME.c_str());
    m_libHandle = dlopen(SHARED_LIB_NAME.c_str(), RTLD_NOW);
    if (m_libHandle == nullptr) {
        const char *errStr = dlerror();
        ERR("load %s error:%s", SHARED_LIB_NAME.c_str(), (errStr != nullptr) ? errStr : "unknown");
        return false;
    }
    for (auto &symbol : m_funcMap) {
        void *func = dlsym(m_libHandle, symbol.first.c_str());
        if (func == nullptr) {
            ERR("failed to load %s", symbol.first.c_str());
            UnloadNetintSharedLib();
            return false;
        }
        symbol.second = func;
    }
    return true;
}

void VideoEncoderNetint::UnloadNetintSharedLib()
{
    if (m_libHandle != nullptr) {
        DBG("unload %s", SHARED_LIB_NAME.c_str());
        (void) dlclose(m_libHandle);
        m_libHandle = nullptr;
    }
}

bool VideoEncoderNetint::InitCodec()
{
    if (!InitCtxParams()) {
        ERR("init context params failed");
        return false;
    }
    auto deviceSessionContextInit =
        reinterpret_cast<NiDeviceSessionContextInitFunc>(m_funcMap[NI_DEVICE_SESSION_CONTEXT_INIT]);
    (*deviceSessionContextInit)(&m_sessionCtx);
    m_sessionCtx.session_id = NI_INVALID_SESSION_ID;
    m_sessionCtx.codec_format = (m_codec == EN_H264) ? NI_CODEC_FORMAT_H264 : NI_CODEC_FORMAT_H265;
    auto rsrcAllocateAuto = reinterpret_cast<NiRsrcAllocateAutoFunc>(m_funcMap[NI_RSRC_ALLOCATE_AUTO]);
    m_devCtx = (*rsrcAllocateAuto)(NI_DEVICE_TYPE_ENCODER, EN_ALLOC_LEAST_LOAD, m_codec,
        m_encParams.width, m_encParams.height, m_encParams.frameRate, &m_load);
    if (m_devCtx == nullptr) {
        ERR("rsrc allocate auto failed");
        return false;
    }
    std::string xcoderId = m_devCtx->p_device_info->blk_name;
    INFO("netint xcoder id: %s", xcoderId.c_str());
    auto deviceOpen = reinterpret_cast<NiDeviceOpenFunc>(m_funcMap[NI_DEVICE_OPEN]);
    ni_device_handle_t devHandle = (*deviceOpen)(xcoderId.c_str(), &m_sessionCtx.max_nvme_io_size);
    ni_device_handle_t blkHandle = (*deviceOpen)(xcoderId.c_str(), &m_sessionCtx.max_nvme_io_size);
    if ((devHandle == NI_INVALID_DEVICE_HANDLE) || (blkHandle == NI_INVALID_DEVICE_HANDLE)) {
        ERR("device open falied");
        return false;
    }
    m_sessionCtx.device_handle = devHandle;
    m_sessionCtx.blk_io_handle = blkHandle;
    m_sessionCtx.hw_id = 0;
    m_sessionCtx.p_session_config = &m_niEncParams;
    m_sessionCtx.src_bit_depth = BIT_DEPTH;
    m_sessionCtx.src_endian = NI_LITTLE_ENDIAN_PLATFORM;
    m_sessionCtx.bit_depth_factor = 1;
    return true;
}

bool VideoEncoderNetint::InitCtxParams()
{
    const uint32_t defaultBitrate =
        (m_codec == EN_H264) ? 5000000 : 3000000;  // h.264: 5Mbps, h.265: 3Mbps
    auto encInitDefaultParams = reinterpret_cast<NiEncInitDefaultParamsFunc>(m_funcMap[NI_ENCODER_INIT_DEFAULT_PARAMS]);
    ni_retcode_t ret = (*encInitDefaultParams)(&m_niEncParams, m_encParams.frameRate, 1,
        defaultBitrate, m_encParams.width, m_encParams.height);
    if (ret != NI_RETCODE_SUCCESS) {
        ERR("encoder init default params error %d", ret);
        return false;
    }
    const std::string gopPresetOpt = "gopPresetIdx";
    const std::string lowDelayOpt = "lowDelay";
    const std::string rateControlOpt = "RcEnable";
    const std::string profileOpt = "profile";
    const std::string noBframeOption = "2";
    const std::string enableOption = "1";
    const std::string baselineOption = "1";
    std::unordered_map<std::string, std::string> xcoderParams = {
        { gopPresetOpt, noBframeOption },  // GOP: IPPP...
        { lowDelayOpt, enableOption },     // enable low delay mode
        { rateControlOpt, enableOption },  // rate control enable
        { profileOpt, baselineOption }     // profile: Baseline(h.264), Main(h.265)
    };
    auto encParamsSetValue = reinterpret_cast<NiEncParamsSetValueFunc>(m_funcMap[NI_ENCODER_PARAMS_SET_VALUE]);
    for (const auto &xParam : xcoderParams) {
        ret = (*encParamsSetValue)(&m_niEncParams, xParam.first.c_str(), xParam.second.c_str());
        if (ret != NI_RETCODE_SUCCESS) {
            ERR("encoder params set value error %d: name %s : value %s",
                ret, xParam.first.c_str(), xParam.second.c_str());
            return false;
        }
    }
    if (m_widthAlign > m_width) {
        m_niEncParams.hevc_enc_params.conf_win_right += m_widthAlign - m_width;
        INFO("YUV width aligned adjustment, from %d to %d, win rignt = %d",
            m_width, m_widthAlign, m_niEncParams.hevc_enc_params.conf_win_right);
        m_niEncParams.source_width = m_widthAlign;
    }
    if (m_heightAlign > m_height) {
        m_niEncParams.hevc_enc_params.conf_win_bottom += m_heightAlign - m_height;
        INFO("YUV height aligned adjustment, from %d to %d, win bottom = %d",
            m_height, m_heightAlign, m_niEncParams.hevc_enc_params.conf_win_bottom);
        m_niEncParams.source_height = m_heightAlign;
    }
    m_niEncParams.hevc_enc_params.intra_period = GOP_SIZE_DEFAULT;
    return true;
}

EncoderRetCode VideoEncoderNetint::StartEncoder()
{
    INFO("start encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderNetint::EncodeOneFrame(const uint8_t *inputData, uint32_t inputSize,
    uint8_t **outputData, uint32_t *outputSize)
{
    if (inputSize < static_cast<uint32_t>(m_width * m_height * NUM_OF_PLANES / COMPRESS_RATIO)) {
        ERR("input size error: size(%u) < frame size", inputSize);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    if (m_resetFlag) {
        if (ResetEncoder() != VIDEO_ENCODER_SUCCESS) {
            ERR("reset encoder failed while encoding");
            return VIDEO_ENCODER_ENCODE_FAIL;
        }
        m_resetFlag = false;
    }

    if (!InitFrameData(inputData)) {
        return VIDEO_ENCODER_ENCODE_FAIL;
    }

    DBG("===> encoder send data begin <===");
    auto deviceSessionWrite = reinterpret_cast<NiDeviceSessionWriteFunc>(m_funcMap[NI_DEVICE_SESSION_WRITE]);
    int oneSent = 0;
    uint32_t sentCnt = 0;
    constexpr int maxSentTimes = 3;
    while (oneSent == 0 && sentCnt < maxSentTimes) {
        oneSent = (*deviceSessionWrite)(&m_sessionCtx, &m_frame, NI_DEVICE_TYPE_ENCODER);
        ++sentCnt;
    }
    if (oneSent < 0 || sentCnt == maxSentTimes) {
        ERR("device session write error, return sent size = %d", oneSent);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    ni_frame_t dataFrame = m_frame.data.frame;
    uint32_t sentBytes = dataFrame.data_len[Y_INDEX] + dataFrame.data_len[U_INDEX] + dataFrame.data_len[V_INDEX];
    DBG("encoder send data success, total sent data size = %u", sentBytes);

    DBG("===> encoder receive data begin <===");
    ni_packet_t *dataPacket = &(m_packet.data.packet);
    auto packetBufferAlloc = reinterpret_cast<NiPacketBufferAllocFunc>(m_funcMap[NI_PACKET_BUFFER_ALLOC]);
    ni_retcode_t ret = (*packetBufferAlloc)(dataPacket, NI_MAX_TX_SZ);
    if (ret != NI_RETCODE_SUCCESS) {
        ERR("packet buffer alloc error %d", ret);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    auto deviceSessionRead = reinterpret_cast<NiDeviceSessionReadFunc>(m_funcMap[NI_DEVICE_SESSION_READ]);
    int oneRead = (*deviceSessionRead)(&m_sessionCtx, &m_packet, NI_DEVICE_TYPE_ENCODER);
    DBG("encoder receive data: total received data size = %d", oneRead);
    const int metaDataSize = NI_FW_ENC_BITSTREAM_META_DATA_SIZE;
    if (oneRead > metaDataSize) {
        if (m_sessionCtx.pkt_num == 0) {
            m_sessionCtx.pkt_num = 1;
        }
    } else if (oneRead != 0) {
        ERR("received %d bytes <= metadata size %d", oneRead, metaDataSize);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    DBG("encoder receive data success");

    *outputData = static_cast<uint8_t *>(dataPacket->p_data) + metaDataSize;
    *outputSize = static_cast<uint32_t>(dataPacket->data_len - metaDataSize);
    return VIDEO_ENCODER_SUCCESS;
}

bool VideoEncoderNetint::InitFrameData(const uint8_t *src)
{
    if (src == nullptr) {
        ERR("input data buffer is null");
        return false;
    }
    ni_frame_t *dataFrame = &(m_frame.data.frame);
    dataFrame->start_of_stream = 0;
    dataFrame->end_of_stream = 0;
    dataFrame->force_key_frame = 0;
    dataFrame->video_width = m_width;
    dataFrame->video_height = m_height;
    dataFrame->extra_data_len = NI_APP_ENC_FRAME_META_DATA_SIZE;

    int dstPlaneStride[NUM_OF_PLANES] = {0};
    int dstPlaneHeight[NUM_OF_PLANES] = {0};
    auto getHwYuv420pDim = reinterpret_cast<NiGetHwYuv420pDimFunc>(m_funcMap[NI_GET_HW_YUV420P_DIM]);
    (*getHwYuv420pDim)(m_width, m_height, m_sessionCtx.bit_depth_factor,
        m_sessionCtx.codec_format == NI_CODEC_FORMAT_H264, dstPlaneStride, dstPlaneHeight);

    auto frameBufferAllocV3 = reinterpret_cast<NiFrameBufferAllocV3Func>(m_funcMap[NI_FRAME_BUFFER_ALLOC_V3]);
    (*frameBufferAllocV3)(dataFrame, m_width, m_height, dstPlaneStride,
        m_sessionCtx.codec_format == NI_CODEC_FORMAT_H264, dataFrame->extra_data_len);
    if (!dataFrame->p_data[Y_INDEX]) {
        ERR("frame buffer alloc failed");
        return false;
    }
    int srcPlaneStride[NUM_OF_PLANES] = { m_width, m_width / COMPRESS_RATIO, m_width / COMPRESS_RATIO };
    int srcPlaneHeight[NUM_OF_PLANES] = { m_height, m_height / COMPRESS_RATIO, m_height / COMPRESS_RATIO };
    uint8_t *srcPlanes[NUM_OF_PLANES];
    srcPlanes[Y_INDEX] = const_cast<uint8_t*>(src);
    srcPlanes[U_INDEX] = srcPlanes[Y_INDEX] + srcPlaneStride[Y_INDEX] * srcPlaneHeight[Y_INDEX];
    srcPlanes[V_INDEX] = srcPlanes[U_INDEX] + srcPlaneStride[U_INDEX] * srcPlaneHeight[U_INDEX];

    auto copyHwYuv420p = reinterpret_cast<NiCopyHwYuv420pFunc>(m_funcMap[NI_COPY_HW_YUV420P]);
    (*copyHwYuv420p)((uint8_t**)(dataFrame->p_data), srcPlanes, m_width, m_height, m_sessionCtx.bit_depth_factor,
        dstPlaneStride, dstPlaneHeight, srcPlaneStride, srcPlaneHeight);
    return true;
}

EncoderRetCode VideoEncoderNetint::StopEncoder()
{
    INFO("stop encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

void VideoEncoderNetint::DestroyEncoder()
{
    DBG("destroy encoder start");
    if (m_libHandle == nullptr) {
        WARN("encoder has been destroyed");
        return;
    }
    auto deviceSessionClose = reinterpret_cast<NiDeviceSessionCloseFunc>(m_funcMap[NI_DEVICE_SESSION_CLOSE]);
    (void) (*deviceSessionClose)(&m_sessionCtx, 1, NI_DEVICE_TYPE_ENCODER);
    if (m_devCtx != nullptr) {
        DBG("destroy rsrc start");
        auto rsrcReleaseResource = reinterpret_cast<NiRsrcReleaseResourceFunc>(m_funcMap[NI_RSRC_RELEASE_RESOURCE]);
        (*rsrcReleaseResource)(m_devCtx, m_codec, m_load);
        auto rsrcFreeDeviceContext =
            reinterpret_cast<NiRsrcFreeDeviceContextFunc>(m_funcMap[NI_RSRC_FREE_DEVICE_CONTEXT]);
        (*rsrcFreeDeviceContext)(m_devCtx);
        m_devCtx = nullptr;
        DBG("destroy rsrc done");
    }
    auto frameBufferFree = reinterpret_cast<NiFrameBufferFreeFunc>(m_funcMap[NI_FRAME_BUFFER_FREE]);
    (void) (*frameBufferFree)(&(m_frame.data.frame));
    auto packetBufferFree = reinterpret_cast<NiPacketBufferFreeFunc>(m_funcMap[NI_PACKET_BUFFER_FREE]);
    (void) (*packetBufferFree)(&(m_packet.data.packet));
    UnloadNetintSharedLib();
    INFO("destroy encoder done");
}

EncoderRetCode VideoEncoderNetint::ResetEncoder()
{
    INFO("resetting encoder");

    INFO("reset encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderNetint::ForceKeyFrame()
{
    INFO("force key frame success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderNetint::SetEncodeParams(const EncodeParams &encParams)
{
    if (encParams == m_encParams) {
        WARN("encode params are not changed");
        return VIDEO_ENCODER_SUCCESS;
    }
    if (!VerifyEncodeParams(encParams)) {
        ERR("encoder params is not supported");
        return VIDEO_ENCODER_SET_ENCODE_PARAMS_FAIL;
    }
    m_encParams = encParams;
    m_resetFlag = true;
    return VIDEO_ENCODER_SUCCESS;
}
