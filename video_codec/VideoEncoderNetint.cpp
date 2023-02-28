/*
 * 功能说明: 适配NETINT硬件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */

#define LOG_TAG "VideoEncoderNetint"
#include "VideoEncoderNetint.h"
#include <dlfcn.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <atomic>
#include "MediaLog.h"
#include "Property.h"

namespace {
    constexpr int Y_INDEX = 0;
    constexpr int U_INDEX = 1;
    constexpr int V_INDEX = 2;
    constexpr int BIT_DEPTH = 8;
    constexpr int NUM_OF_PLANES = 3;
    constexpr int COMPRESS_RATIO = 2;

    const std::string NI_ENCODER_INIT_DEFAULT_PARAMS = "ni_encoder_init_default_params";
    const std::string NI_ENCODER_PARAMS_SET_VALUE = "ni_encoder_params_set_value";
    const std::string NI_RSRC_ALLOCATE_AUTO = "ni_rsrc_allocate_auto";
    const std::string NI_RSRC_RELEASE_RESOURCE = "ni_rsrc_release_resource";
    const std::string NI_RSRC_FREE_DEVICE_CONTEXT = "ni_rsrc_free_device_context";
    const std::string NI_DEVICE_OPEN = "ni_device_open";
    const std::string NI_DEVICE_CLOSE = "ni_device_close";
    const std::string NI_DEVICE_SESSION_CONTEXT_INIT = "ni_device_session_context_init";
    const std::string NI_DEVICE_SESSION_CONTEXT_FREE = "ni_device_session_context_free";
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
        ni_codec_t codec, int width, int height, int framerate, unsigned long *load);
    using NiRsrcReleaseResourceFunc = void (*)(ni_device_context_t *devCtx, ni_codec_t codec, unsigned long load);
    using NiRsrcFreeDeviceContextFunc = void (*)(ni_device_context_t *devCtx);
    using NiDeviceOpenFunc = ni_device_handle_t (*)(const char *dev, uint32_t *maxIoSizeOut);
    using NiDeviceCloseFunc = void (*)(ni_device_handle_t devHandle);
    using NiDeviceSessionContextInitFunc = void (*)(ni_session_context_t *sessionCtx);
    using NiDeviceSessionContextFreeFunc = void (*)(ni_session_context_t *sessionCtx);
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
        int srcStride[NI_MAX_NUM_DATA_POINTERS], int srcHeight[NI_MAX_NUM_DATA_POINTERS]);

    std::unordered_map<std::string, void*> g_funcMap = {
        { NI_ENCODER_INIT_DEFAULT_PARAMS, nullptr },
        { NI_ENCODER_INIT_DEFAULT_PARAMS, nullptr },
        { NI_ENCODER_PARAMS_SET_VALUE, nullptr },
        { NI_RSRC_ALLOCATE_AUTO, nullptr },
        { NI_RSRC_RELEASE_RESOURCE, nullptr },
        { NI_RSRC_FREE_DEVICE_CONTEXT, nullptr },
        { NI_DEVICE_OPEN, nullptr },
        { NI_DEVICE_CLOSE, nullptr },
        { NI_DEVICE_SESSION_CONTEXT_INIT, nullptr },
        { NI_DEVICE_SESSION_CONTEXT_FREE, nullptr },
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

    std::unordered_map<std::string, std::string> g_transProfile = {
        {"baseline", "66"},
        {"main", "77"},
        {"high", "100"}};

    const std::string SHARED_LIB_NAME = "libxcoder.so";
    std::atomic<bool> g_netintLoaded = { false };
    void *g_libHandle = nullptr;
}

VideoEncoderNetint::VideoEncoderNetint(NiCodecType codecType)
{
    if (codecType == NI_CODEC_TYPE_H264) {
        m_codec = EN_H264;
    } else {
        m_codec = EN_H265;
        m_encParams.bitrate = static_cast<uint32_t>(BITRATE_DEFAULT_265);
        m_encParams.profile = ENCODE_PROFILE_MAIN;
    }
    INFO("VideoEncoderNetint constructed %s", (m_codec == EN_H264) ? "h.264" : "h.265");
}

VideoEncoderNetint::~VideoEncoderNetint()
{
    DestroyEncoder();
    INFO("VideoEncoderNetint destructed");
}

bool VideoEncoderNetint::GetRoEncParam()
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

bool VideoEncoderNetint::GetPersistEncParam()
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

bool VideoEncoderNetint::EncodeParamsChange()
{
    return (m_tmpEncParams.bitrate != m_encParams.bitrate) || (m_tmpEncParams.gopsize != m_encParams.gopsize)
        || (m_tmpEncParams.profile != m_encParams.profile) || (m_tmpEncParams.width != m_encParams.width)
        || (m_tmpEncParams.height != m_encParams.height) || (m_tmpEncParams.framerate != m_encParams.framerate);
}

EncoderRetCode VideoEncoderNetint::InitEncoder()
{
    if ((!GetRoEncParam()) || (!GetPersistEncParam())) {
        ERR("init encoder failed: GetEncParam failed");
        return VIDEO_ENCODER_INIT_FAIL;
    }
    m_encParams = m_tmpEncParams;
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
    auto deviceSessionOpen = reinterpret_cast<NiDeviceSessionOpenFunc>(g_funcMap[NI_DEVICE_SESSION_OPEN]);
    ni_retcode_t ret = (*deviceSessionOpen)(&m_sessionCtx, NI_DEVICE_TYPE_ENCODER);
    if (ret != NI_RETCODE_SUCCESS) {
        ERR("init encoder failed: device session open error %d", ret);
        return VIDEO_ENCODER_INIT_FAIL;
    }
    m_frame.data.frame.start_of_stream = 1;
    m_isInited = true;
    INFO("init encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

bool VideoEncoderNetint::VerifyEncodeRoParams(int32_t width, int32_t height, int32_t framerate)
{
    bool isEncodeParamsTrue = true;
    if (width > NI_PARAM_MAX_WIDTH || height > NI_PARAM_MAX_HEIGHT ||
        width < NI_PARAM_MIN_WIDTH || height < NI_PARAM_MIN_HEIGHT) {
        ERR("Invalid property value[%dx%d] for property[width,height], get property failed!", width, height);
        isEncodeParamsTrue = false;
    }
    if (framerate != FRAMERATE_MIN && framerate != FRAMERATE_MAX) {
        ERR("Invalid property value[%d] for property[framerate], get property failed!", framerate);
        isEncodeParamsTrue = false;
    }
    return isEncodeParamsTrue;
}

bool VideoEncoderNetint::VerifyEncodeParams(std::string &bitrate, std::string &gopsize, std::string &profile)
{
    bool isEncodeParamsTrue = true;
    if ((StrToInt(bitrate) < (int32_t)BITRATE_MIN) || (StrToInt(bitrate) > (int32_t)BITRATE_MAX)) {
        WARN("Invalid property value[%s] for property[bitrate], use last correct encode bitrate[%u]",
            bitrate.c_str(), m_encParams.bitrate);
        isEncodeParamsTrue = false;
    }
    if ((StrToInt(gopsize) < (int32_t)GOPSIZE_MIN) || (StrToInt(gopsize) > (int32_t)GOPSIZE_MAX)) {
        WARN("Invalid property value[%s] for property[gopsize], use last correct encode gopsize[%u]",
            gopsize.c_str(), m_encParams.gopsize);
        isEncodeParamsTrue = false;
    }
    if (profile != ENCODE_PROFILE_BASELINE &&
        profile != ENCODE_PROFILE_MAIN &&
        profile != ENCODE_PROFILE_HIGH) {
        WARN("Invalid property value[%s] for property[profile], use last correct encode profile[%s]",
            profile.c_str(), m_encParams.profile.c_str());
        isEncodeParamsTrue = false;
    }

    return isEncodeParamsTrue;
}

bool VideoEncoderNetint::LoadNetintSharedLib()
{
    if (g_netintLoaded) {
        return true;
    }
    INFO("load %s", SHARED_LIB_NAME.c_str());
    g_libHandle = dlopen(SHARED_LIB_NAME.c_str(), RTLD_LAZY);
    if (g_libHandle == nullptr) {
        const char *errStr = dlerror();
        ERR("load %s error:%s", SHARED_LIB_NAME.c_str(), (errStr != nullptr) ? errStr : "unknown");
        return false;
    }
    for (auto &symbol : g_funcMap) {
        void *func = dlsym(g_libHandle, symbol.first.c_str());
        if (func == nullptr) {
            ERR("failed to load %s", symbol.first.c_str());
            return false;
        }
        symbol.second = func;
    }
    g_netintLoaded = true;
    return true;
}

bool VideoEncoderNetint::InitCodec()
{
    if (!InitCtxParams()) {
        ERR("init context params failed");
        return false;
    }
    auto deviceSessionContextInit =
        reinterpret_cast<NiDeviceSessionContextInitFunc>(g_funcMap[NI_DEVICE_SESSION_CONTEXT_INIT]);
    (*deviceSessionContextInit)(&m_sessionCtx);
    m_sessionCtx.session_id = NI_INVALID_SESSION_ID;
    m_sessionCtx.codec_format = (m_codec == EN_H264) ? NI_CODEC_FORMAT_H264 : NI_CODEC_FORMAT_H265;
    auto rsrcAllocateAuto = reinterpret_cast<NiRsrcAllocateAutoFunc>(g_funcMap[NI_RSRC_ALLOCATE_AUTO]);
    m_devCtx = (*rsrcAllocateAuto)(NI_DEVICE_TYPE_ENCODER, EN_ALLOC_LEAST_LOAD, m_codec,
        m_encParams.width, m_encParams.height, m_encParams.framerate, &m_load);
    if (m_devCtx == nullptr) {
        ERR("rsrc allocate auto failed");
        return false;
    }
    std::string xcoderId = m_devCtx->p_device_info->blk_name;
    INFO("netint xcoder id: %s", xcoderId.c_str());
    auto deviceOpen = reinterpret_cast<NiDeviceOpenFunc>(g_funcMap[NI_DEVICE_OPEN]);
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
    auto encInitDefaultParams = reinterpret_cast<NiEncInitDefaultParamsFunc>(g_funcMap[NI_ENCODER_INIT_DEFAULT_PARAMS]);
    ni_retcode_t ret = (*encInitDefaultParams)(
        &m_niEncParams, m_encParams.framerate, 1, m_encParams.bitrate, m_encParams.width, m_encParams.height);
    if (ret != NI_RETCODE_SUCCESS) {
        ERR("encoder init default params error %d", ret);
        return false;
    }
    const std::string gopPresetOpt = "gopPresetIdx";
    const std::string lowDelayOpt = "lowDelay";
    const std::string rateControlOpt = "RcEnable";
    const std::string profileOpt = "profile";
    const std::string lowDelayPocTypeOpt = "useLowDelayPocType";
    const std::string noBframeOption = "2";
    const std::string enableOption = "1";

    std::unordered_map<std::string, std::string> xcoderParams = {
        { gopPresetOpt, noBframeOption },   // GOP: IPPP...
        { lowDelayOpt, enableOption },      // enable low delay mode
        { rateControlOpt, enableOption },   // rate control enable
        { profileOpt, m_encParams.profile },     // profile: Baseline(h.264), Main(h.265)
        { lowDelayPocTypeOpt, enableOption} // enable lowDelayPoc
    };
    auto encParamsSetValue = reinterpret_cast<NiEncParamsSetValueFunc>(g_funcMap[NI_ENCODER_PARAMS_SET_VALUE]);
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
    m_niEncParams.hevc_enc_params.intra_period = m_encParams.gopsize;
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
    uint32_t frameSize = static_cast<uint32_t>(m_width * m_height * NUM_OF_PLANES / COMPRESS_RATIO);
    if (inputSize < frameSize) {
        ERR("input size error: size(%u) < frame size(%u)", inputSize, frameSize);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }

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

    if (m_resetFlag) {
        if (ResetEncoder() != VIDEO_ENCODER_SUCCESS) {
            ERR("reset encoder failed while encoding");
            return VIDEO_ENCODER_ENCODE_FAIL;
        }
        m_resetFlag = false;
    }

    std::string isKeyframeChange = GetStrEncParam("persist.vmi.video.encode.keyframe");
    if (isKeyframeChange == "1") {
        INFO("Encoder set key frame");
        ForceKeyFrame();
        SetEncParam("persist.vmi.video.encode.keyframe", "0");
    } else if (isKeyframeChange != "0") {
        WARN("Invalid property value[%s] for property[keyFrame], set to [0]", isKeyframeChange.c_str());
        SetEncParam("persist.vmi.video.encode.keyframe", "0");
    }

    if (!InitFrameData(inputData)) {
        return VIDEO_ENCODER_ENCODE_FAIL;
    }

    DBG("===> encoder send data begin <===");
    auto deviceSessionWrite = reinterpret_cast<NiDeviceSessionWriteFunc>(g_funcMap[NI_DEVICE_SESSION_WRITE]);
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
    auto packetBufferAlloc = reinterpret_cast<NiPacketBufferAllocFunc>(g_funcMap[NI_PACKET_BUFFER_ALLOC]);
    ni_retcode_t ret = (*packetBufferAlloc)(dataPacket, frameSize);
    if (ret != NI_RETCODE_SUCCESS) {
        ERR("packet buffer alloc error %d", ret);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    auto deviceSessionRead = reinterpret_cast<NiDeviceSessionReadFunc>(g_funcMap[NI_DEVICE_SESSION_READ]);
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
    auto getHwYuv420pDim = reinterpret_cast<NiGetHwYuv420pDimFunc>(g_funcMap[NI_GET_HW_YUV420P_DIM]);
    (*getHwYuv420pDim)(m_width, m_height, m_sessionCtx.bit_depth_factor,
        m_sessionCtx.codec_format == NI_CODEC_FORMAT_H264, dstPlaneStride, dstPlaneHeight);

    auto frameBufferAllocV3 = reinterpret_cast<NiFrameBufferAllocV3Func>(g_funcMap[NI_FRAME_BUFFER_ALLOC_V3]);
    ni_retcode_t ret = (*frameBufferAllocV3)(dataFrame, m_width, m_height, dstPlaneStride,
        m_sessionCtx.codec_format == NI_CODEC_FORMAT_H264, dataFrame->extra_data_len);
    if (ret != NI_RETCODE_SUCCESS || !dataFrame->p_data[Y_INDEX]) {
        ERR("frame buffer alloc failed: ret = %d", ret);
        return false;
    }
    int srcPlaneStride[NUM_OF_PLANES] = { m_width, m_width / COMPRESS_RATIO, m_width / COMPRESS_RATIO };
    int srcPlaneHeight[NUM_OF_PLANES] = { m_height, m_height / COMPRESS_RATIO, m_height / COMPRESS_RATIO };
    uint8_t *srcPlanes[NUM_OF_PLANES];
    srcPlanes[Y_INDEX] = const_cast<uint8_t*>(src);
    srcPlanes[U_INDEX] = srcPlanes[Y_INDEX] + srcPlaneStride[Y_INDEX] * srcPlaneHeight[Y_INDEX];
    srcPlanes[V_INDEX] = srcPlanes[U_INDEX] + srcPlaneStride[U_INDEX] * srcPlaneHeight[U_INDEX];

    auto copyHwYuv420p = reinterpret_cast<NiCopyHwYuv420pFunc>(g_funcMap[NI_COPY_HW_YUV420P]);
    (*copyHwYuv420p)((uint8_t**)(dataFrame->p_data), srcPlanes, m_width, m_height, m_sessionCtx.bit_depth_factor,
        dstPlaneStride, dstPlaneHeight, srcPlaneStride, srcPlaneHeight);
    return true;
}

EncoderRetCode VideoEncoderNetint::StopEncoder()
{
    INFO("stop encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

void VideoEncoderNetint::CheckFuncPtr()
{
    for (auto &symbol : g_funcMap) {
        if (symbol.second == nullptr) {
            m_FunPtrError = true;
            ERR("%s ptr is nullptr", symbol.first.c_str());
        }
    }
}

void VideoEncoderNetint::DestroyEncoder()
{
    if (!m_isInited) {
        INFO("Destroy encoder already triggered, return");
        return;
    }
    INFO("destroy encoder start");
    if (g_libHandle == nullptr) {
        WARN("encoder has been destroyed");
        return;
    }
    CheckFuncPtr();
    ni_retcode_t ret;
    if (g_funcMap[NI_DEVICE_SESSION_CLOSE] != nullptr) {
        auto deviceSessionClose = reinterpret_cast<NiDeviceSessionCloseFunc>(g_funcMap[NI_DEVICE_SESSION_CLOSE]);
        ret = (*deviceSessionClose)(&m_sessionCtx, 1, NI_DEVICE_TYPE_ENCODER);
        if (ret != NI_RETCODE_SUCCESS) {
            WARN("device session close failed: ret = %d", ret);
        }
    }
    if (g_funcMap[NI_DEVICE_CLOSE] != nullptr) {
        auto deviceClose = reinterpret_cast<NiDeviceCloseFunc>(g_funcMap[NI_DEVICE_CLOSE]);
        (*deviceClose)(m_sessionCtx.device_handle);
        (*deviceClose)(m_sessionCtx.blk_io_handle);
    }
    if (m_devCtx != nullptr) {
        INFO("destroy rsrc start");
        if (g_funcMap[NI_RSRC_RELEASE_RESOURCE] != nullptr) {
            auto rsrcReleaseResource = reinterpret_cast<NiRsrcReleaseResourceFunc>(g_funcMap[NI_RSRC_RELEASE_RESOURCE]);
            (*rsrcReleaseResource)(m_devCtx, m_codec, m_load);
        }
        if (g_funcMap[NI_RSRC_FREE_DEVICE_CONTEXT] != nullptr) {
            auto rsrcFreeDeviceContext =
            reinterpret_cast<NiRsrcFreeDeviceContextFunc>(g_funcMap[NI_RSRC_FREE_DEVICE_CONTEXT]);
            (*rsrcFreeDeviceContext)(m_devCtx);
        }
        m_devCtx = nullptr;
        INFO("destroy rsrc done");
    }
    if (g_funcMap[NI_DEVICE_SESSION_CONTEXT_FREE] != nullptr) {
        auto deviceSessionContextFree =
            reinterpret_cast<NiDeviceSessionContextFreeFunc>(g_funcMap[NI_DEVICE_SESSION_CONTEXT_FREE]);
        (*deviceSessionContextFree)(&m_sessionCtx);
    }
    if (g_funcMap[NI_FRAME_BUFFER_FREE] != nullptr) {
        auto frameBufferFree = reinterpret_cast<NiFrameBufferFreeFunc>(g_funcMap[NI_FRAME_BUFFER_FREE]);
        ret = (*frameBufferFree)(&(m_frame.data.frame));
        if (ret != NI_RETCODE_SUCCESS) {
            WARN("device session close failed: ret = %d", ret);
        }
    }
    if (g_funcMap[NI_PACKET_BUFFER_FREE] != nullptr) {
        auto packetBufferFree = reinterpret_cast<NiPacketBufferFreeFunc>(g_funcMap[NI_PACKET_BUFFER_FREE]);
        ret = (*packetBufferFree)(&(m_packet.data.packet));
        if (ret != NI_RETCODE_SUCCESS) {
            WARN("device session close failed: ret = %d", ret);
        }
    }
    if (m_FunPtrError) {
        UnLoadNetintSharedLib();
    }
    m_isInited = false;
    INFO("destroy encoder done");
}

void VideoEncoderNetint::UnLoadNetintSharedLib()
{
    INFO("UnLoadNetintSharedLib");
    for (auto &symbol : g_funcMap) {
        symbol.second = nullptr;
    }
    (void)dlclose(g_libHandle);
    g_libHandle = nullptr;
    g_netintLoaded = false;
    m_FunPtrError = false;
}

EncoderRetCode VideoEncoderNetint::ResetEncoder()
{
    INFO("resetting encoder");
    DestroyEncoder();
    EncoderRetCode ret = InitEncoder();
    if (ret != VIDEO_ENCODER_SUCCESS) {
        ERR("init encoder failed %#x while resetting", ret);
        return VIDEO_ENCODER_RESET_FAIL;
    }
    ret = StartEncoder();
    if (ret != VIDEO_ENCODER_SUCCESS) {
        ERR("start encoder failed %#x while resetting", ret);
        return VIDEO_ENCODER_RESET_FAIL;
    }
    INFO("reset encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderNetint::ForceKeyFrame()
{
    INFO("force key frame success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderNetint::SetEncodeParams()
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
