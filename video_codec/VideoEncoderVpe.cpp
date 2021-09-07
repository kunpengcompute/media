/*
 * 版权所有 (c) 华为技术有限公司 2021-2021
 * 功能说明: 适配涌现科技硬件视频编码器，包括编码器初始化、启动、编码、停止、销毁等
 */

#define LOG_TAG "VideoEncoderVpe"
#include "VideoEncoderVpe.h"

#include <string>
#include <cstring>
#include <unordered_map>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>

#include "MediaLog.h"

namespace {
    constexpr uint32_t FRAMERATE_MIN  = 30;
    constexpr uint32_t FRAMERATE_MAX  = 60;
    constexpr uint32_t GOPSIZE_MIN    = 30;
    constexpr uint32_t GOPSIZE_MAX    = 3000;
    constexpr uint32_t BITRATE_MIN    = 100000;
    constexpr uint32_t BITRATE_MAX    = 100000000;
    constexpr uint32_t VPE_WIDTH_MAX  = 4096;
    constexpr uint32_t VPE_HEIGHT_MAX = 4096;
    constexpr uint32_t VPE_WIDTH_MIN  = 144;
    constexpr uint32_t VPE_HEIGHT_MIN = 144;
    constexpr uint32_t Y_INDEX        = 0;
    constexpr uint32_t U_INDEX        = 1;
    constexpr uint32_t V_INDEX        = 2;
    constexpr uint32_t NUM_OF_PLANES  = 3;
    constexpr uint32_t COMPRESS_RATIO = 2;
    constexpr int MEM_ALLOCATION_RESERVE      = 300;
    const std::string DEV_NAME_PREFIX         = "transcoder";
    const std::string INFO_PATH_PREFIX        = "/sys/class/misc/transcoder";
    const std::string ENC_UTIL                = "enc_util";
    const std::string MEM_USAGE               = "mem_info";
    const std::string POWER_STATUS            = "power_state";
    const std::string VPI_OPEN_HWDEVICE       = "vpi_open_hwdevice";
    const std::string VPI_CLOSE_HWDEVICE      = "vpi_close_hwdevice";
    const std::string VPI_CREATE              = "vpi_create";
    const std::string VPI_FREEP               = "vpi_freep";
    const std::string VPI_DESTROY             = "vpi_destroy";
    const std::string VPI_GET_SYS_INFO_STRUCT = "vpi_get_sys_info_struct";
    const std::string VPI_FRAME_RELEASE       = "vpi_frame_release";

    using VpiOpenHwDeviceFunc     = int (*)(const char *device);
    using VpiCloseHwDeviceFunc    = int (*)(int fd);
    using VpiCreateFunc           = int (*)(VpiCtx *ctx, VpiApi **vpi, int fd, VpiPlugin plugin);
    using VpiFreepFunc            = void (*)(void *arg);
    using VpiDestroyFunc          = int (*)(VpiCtx ctx, int fd);
    using VpiGetSysInfoStructFunc = int (*)(VpiSysInfo **info);
    using VpiFrameReleaseFunc     = void (*)(VpiFrame *frame);

    std::unordered_map<std::string, void*> g_funcMap = {
        { VPI_OPEN_HWDEVICE, nullptr },
        { VPI_CLOSE_HWDEVICE, nullptr },
        { VPI_CREATE, nullptr },
        { VPI_FREEP, nullptr },
        { VPI_DESTROY, nullptr },
        { VPI_GET_SYS_INFO_STRUCT, nullptr },
        { VPI_FRAME_RELEASE, nullptr },
    };
    const std::string SHARED_LIB_NAME = "libvpi.so";
    std::atomic<bool> g_vpeLoaded = { false };
    void *g_libHandle = nullptr;
}

VideoEncoderVpe::VideoEncoderVpe(VpiH26xCodecID id)
{
    m_codec = id;
    INFO("VideoEncoderVpe constructed %s", (m_codec == CODEC_ID_H264) ? "h.264" : "h.265");
}

VideoEncoderVpe::~VideoEncoderVpe()
{
    DestroyEncoder();
    INFO("VideoEncoderVpe destructed");
}

bool VideoEncoderVpe::CreateVpeDevice()
{
    VpeDevCtx *devCtx = &m_vpeContext.devCtx;
    if (devCtx->initialized) {
        WARN("device has been Created");
    }

    int devId = VpeSrm::SrmAllocateResource();
    if (devId < 0) {
        ERR("cannot find valid device");
        return false;
    }
    std::string device = "/dev/" + (DEV_NAME_PREFIX + std::to_string(devId));

    auto vpiOpenHwDevice = reinterpret_cast<VpiOpenHwDeviceFunc>(g_funcMap[VPI_OPEN_HWDEVICE]);
    auto vpiCloseHwDevice = reinterpret_cast<VpiCloseHwDeviceFunc>(g_funcMap[VPI_CLOSE_HWDEVICE]);
    auto vpiGetSysInfoStruct = reinterpret_cast<VpiGetSysInfoStructFunc>(g_funcMap[VPI_GET_SYS_INFO_STRUCT]);
    auto vpiCreate = reinterpret_cast<VpiCreateFunc>(g_funcMap[VPI_CREATE]);
    auto vpiFreep = reinterpret_cast<VpiFreepFunc>(g_funcMap[VPI_FREEP]);
    devCtx->device = (*vpiOpenHwDevice)(device.c_str());
    if (devCtx->device < 0) {
        ERR("failed tot open hw device %s, ret = %d", device.c_str(), devCtx->device);
        return false;
    }

    if ((*vpiGetSysInfoStruct)(&m_vpeContext.sysInfo) != 0 || m_vpeContext.sysInfo ==nullptr) {
        ERR("failed to get sys info struct");
        (*vpiCloseHwDevice)(devCtx->device);
        return false;
    }
    m_vpeContext.sysInfo->device = devCtx->device;
    m_vpeContext.sysInfo->priority = VPE_TASK_LIVE;
    m_vpeContext.sysInfo->sys_log_level = 0;
    if ((*vpiCreate)(reinterpret_cast<VpiCtx*>(&m_vpeContext.sysInfo), &devCtx->func,
        devCtx->device, HWCONTEXT_VPE) != 0) {
        ERR("failed to create vpe hw context");
        (*vpiFreep)(&m_vpeContext.sysInfo);
        (*vpiCloseHwDevice)(devCtx->device);
        return false;
    }
    m_frameCtx = std::make_unique<VpiFrame>();
    m_vpeContext.frameCtx = m_frameCtx.get();

    VpiCtrlCmdParam cmd;
    cmd.cmd = VPI_CMD_SET_VPEFRAME;
    cmd.data = m_vpeContext.frameCtx;
    devCtx->func->control(&devCtx->device, &cmd, nullptr);
    devCtx->initialized = true;
    return true;
}

void VideoEncoderVpe::DestoryVpeDevice()
{
    VpeDevCtx *devCtx = &m_vpeContext.devCtx;
    if (!devCtx->initialized) {
        WARN("vpe device has been destroy");
        return;
    }
    VpiCtrlCmdParam cmd = {.cmd = VPI_CMD_REMOVE_VPEFRAME, .data = m_vpeContext.frameCtx};
    devCtx->func->control(&devCtx->device, &cmd, nullptr);
    auto vpiFreep = reinterpret_cast<VpiFreepFunc>(g_funcMap[VPI_FREEP]);
    auto vpiDestroy= reinterpret_cast<VpiDestroyFunc>(g_funcMap[VPI_DESTROY]);
    (*vpiDestroy)(m_vpeContext.sysInfo, devCtx->device);
    (*vpiFreep)(&m_vpeContext.sysInfo);
    if (devCtx->device >= 0) {
        auto vpiCloseHwDevice = reinterpret_cast<VpiCloseHwDeviceFunc>(g_funcMap[VPI_CLOSE_HWDEVICE]);
        int ret = (*vpiCloseHwDevice)(devCtx->device);
        INFO("vpe close hwdevice done, ret = %d", ret);
    }
    devCtx->initialized = false;
}


bool VideoEncoderVpe::InitVpeFilter()
{
    VpePPCtx *ppCtx = &m_vpeContext.ppCtx;
    VpeDevCtx *devCtx = &m_vpeContext.devCtx;
    if (ppCtx->initialized) {
        WARN("pp context has been initialized");
        return true;
    }
    auto vpiCreate = reinterpret_cast<VpiCreateFunc>(g_funcMap[VPI_CREATE]);
    auto vpiDestroy= reinterpret_cast<VpiDestroyFunc>(g_funcMap[VPI_DESTROY]);
    if ((*vpiCreate)(&ppCtx->ctx, &ppCtx->vpi, devCtx->device, PP_VPE) != 0) {
        ERR("failed to create pp context");
        return false;
    }

    // get pp option struct
    VpiCtrlCmdParam cmd;
    cmd.cmd = VPI_CMD_PP_INIT_OPTION;
    if (ppCtx->vpi->control(ppCtx->ctx, &cmd, &ppCtx->option) != 0) {
        ERR("failed to get pp option struct");
        (*vpiDestroy)(ppCtx->ctx, devCtx->device);
        return false;
    }
    VpiPPOption *option = static_cast<VpiPPOption*>(ppCtx->option);
    option->w = m_width;
    option->h = m_height;
    option->format = VPI_FMT_YUV420P;
    option->nb_outputs = 1;
    option->force_10bit = 0;
    option->low_res = nullptr;
    option->b_disable_tcache = 0;
    option->frame = m_vpeContext.frameCtx;
    if (ppCtx->vpi->init(ppCtx->ctx, option) != 0) {
        ERR("init pp plugin failed");
        ppCtx->vpi->close(ppCtx->ctx);
        (*vpiDestroy)(ppCtx->ctx, devCtx->device);
        return false;
    }
    ppCtx->initialized = true;
    return true;
}

void VideoEncoderVpe::CloseVpeFilter()
{
    VpePPCtx*ppCtx =  &m_vpeContext.ppCtx;
    VpeDevCtx *devCtx = &m_vpeContext.devCtx;
    if (!ppCtx->initialized) {
        WARN("pp context not initialized yet, skip close vpe filter");
        return;
    }

    ppCtx->vpi->close(ppCtx->ctx);
    auto vpiDestroy= reinterpret_cast<VpiDestroyFunc>(g_funcMap[VPI_DESTROY]);
    (*vpiDestroy)(ppCtx->ctx, devCtx->device);
    ppCtx->initialized = false;
}

bool VideoEncoderVpe::InitVpeCodec()
{
    VpeCodecCtx *encCtx = &m_vpeContext.encCtx;
    VpeDevCtx *devCtx = &m_vpeContext.devCtx;
    auto vpiCreate = reinterpret_cast<VpiCreateFunc>(g_funcMap[VPI_CREATE]);
    auto vpiDestroy= reinterpret_cast<VpiDestroyFunc>(g_funcMap[VPI_DESTROY]);
    if ((*vpiCreate)(&encCtx->ctx, &encCtx->vpi, devCtx->device, H26XENC_VPE) != 0) {
        ERR("create vpe codec failed");
        return false;
    }
    if (!CreateVpeParamsList()) {
        ERR("create vpe params list failed");
        (*vpiDestroy)(encCtx->ctx, devCtx->device);
        return false;
    }
    // get encoder config struct
    VpiCtrlCmdParam cmd = {.cmd = VPI_CMD_ENC_INIT_OPTION, .data = nullptr};
    if (encCtx->vpi->control(encCtx->ctx, &cmd, &encCtx->config) != 0) {
        ERR("get encoder init option struct failed");
        ReleaseVpeParamsList();
        (*vpiDestroy)(encCtx->ctx, devCtx->device);
        return false;
    }
    std::string preset = "superfast";
    std::string profile = "main";
    std::string interval = "interval:" + std::to_string(m_encParams.gopSize);
    VpiH26xEncCfg *h26xEncCfg    = encCtx->config;
    h26xEncCfg->codec_id         = m_codec;
    h26xEncCfg->preset           = const_cast<char*>(preset.c_str());
    h26xEncCfg->profile          = const_cast<char*>(profile.c_str());
    h26xEncCfg->force_idr        = const_cast<char*>(interval.c_str());
    h26xEncCfg->input_rate_numer = m_encParams.frameRate;
    h26xEncCfg->input_rate_denom = 1;
    h26xEncCfg->bit_per_second   = m_encParams.bitrate;
    h26xEncCfg->lum_width_src    = m_vpeContext.frameCtx->width;
    h26xEncCfg->lum_height_src   = m_vpeContext.frameCtx->height;
    h26xEncCfg->input_format     = VPI_YUV420_PLANAR;
    h26xEncCfg->frame_ctx        = m_vpeContext.frameCtx;
    h26xEncCfg->param_list       = encCtx->paramList;
    h26xEncCfg->colour_primaries = h26xEncCfg->transfer_characteristics = h26xEncCfg->matrix_coeffs = COMPRESS_RATIO;
    // Call vpe h26x encoder initialization
    int ret  = encCtx->vpi->init(encCtx->ctx, h26xEncCfg);
    if (ret < 0) {
        ERR("vpe_h26x_encode_init failed, error=(%d)", ret);
        free(encCtx->config);
        ReleaseVpeParamsList();
        (*vpiDestroy)(encCtx->ctx, devCtx->device);
    }
    cmd.cmd = VPI_CMD_ENC_REG_FREE_FRAME;
    cmd.data = g_funcMap[VPI_FRAME_RELEASE];
    if (encCtx->vpi->control(encCtx->ctx, (void*)&cmd, nullptr) != 0) {
        ERR("set vpe frame release function failed");
        return false;
    }
    encCtx->initialized = true;
    return true;
}

void VideoEncoderVpe::FlushVpeCodec()
{
    VpeCodecCtx *encCtx = &m_vpeContext.encCtx;
    VpiFrame *vpiFrame = nullptr;

    if (encCtx == nullptr || encCtx->ctx == nullptr) {
        WARN("encode context has been destroyed, skip flush vpe codec");
        return ;
    }

    VpiCtrlCmdParam cmd = {.cmd = VPI_CMD_ENC_GET_EMPTY_FRAME_SLOT, .data = nullptr};
    if (encCtx->vpi->control(encCtx->ctx, &cmd, &vpiFrame) != 0 || vpiFrame == nullptr) {
        ERR("get empty frame slot failed");
        return;
    }
    memset(vpiFrame, 0, sizeof(VpiFrame));
    int ret = encCtx->vpi->encode_put_frame(encCtx->ctx, vpiFrame);
    if (ret != 0) {
        ERR("flush frame failed, error=(%d)", ret);
    }
}

void VideoEncoderVpe::CloseVpeCodec()
{
    VpeCodecCtx *encCtx = &m_vpeContext.encCtx;
    VpeDevCtx *devCtx = &m_vpeContext.devCtx;
    auto vpiDestroy= reinterpret_cast<VpiDestroyFunc>(g_funcMap[VPI_DESTROY]);

    if (!encCtx->initialized) {
        WARN("encode context not initialized yet, skip clost vpe codec");
        return;
    }

    ReleaseVpeParamsList();
    if (encCtx->ctx != nullptr) {
        encCtx->vpi->close(encCtx->ctx);
        if ((*vpiDestroy)(encCtx->ctx, devCtx->device) != 0) {
            ERR("destory encoder config context failed");
            return;
        }
        encCtx->ctx = nullptr;
    }
    if (encCtx->config != nullptr) {
        free(encCtx->config);
        DBG("free encoder config");
        encCtx->config = nullptr;
    }
    encCtx->initialized = false;
}

bool VideoEncoderVpe::CreateVpeParamsList()
{
    VpeCodecCtx *encCtx = &m_vpeContext.encCtx;
    VpiEncParamSet *tail = nullptr;
    VpiEncParamSet *node = nullptr;

    std::unordered_map<std::string, std::string> vpeEncParams = {
        { "gop_size", "1" },
        { "low_delay", "1" }
    };

    for (const auto &param : vpeEncParams) {
        node = new (std::nothrow) VpiEncParamSet();
        if (node == nullptr) {
            return false;
        }
        node->key = new (std::nothrow) char[param.first.size() + 1];
        if (node->key == nullptr) {
            delete node;
            return false;
        }
        std::copy(param.first.cbegin(), param.first.cend(), node->key);
        node->key[param.first.size()] = '\0';

        node->value = new (std::nothrow) char[param.second.size() + 1];
        if (node->value == nullptr) {
            delete node->key;
            delete node;
            return false;
        }
        std::copy(param.second.cbegin(), param.second.cend(), node->value);
        node->value[param.second.size()] = '\0';

        node->next = nullptr;
        if (tail != nullptr) {
            tail->next = node;
            tail       = node;
        } else {
            encCtx->paramList = tail = node;
        }
    }
    return true;
}

void VideoEncoderVpe::ReleaseVpeParamsList()
{
    VpiEncParamSet *tail = m_vpeContext.encCtx.paramList;
    VpiEncParamSet *node = nullptr;

    while (tail != nullptr) {
        node = tail->next;
        if (tail->key != nullptr) {
            delete [] tail->key;
        }
        if (tail->value != nullptr) {
            delete [] tail->value;
        }
        delete tail;
        tail = node;
    }
}

EncoderRetCode VideoEncoderVpe::InitEncoder(const EncodeParams &encParams)
{
    if (!VerifyEncodeParams(encParams)) {
        ERR("init encoder failed: encoder params is not supported");
        return VIDEO_ENCODER_INIT_FAIL;
    }

    m_encParams = encParams;
    m_width = m_encParams.width;
    m_height = m_encParams.height;

    if (!LoadVpeSharedLib()) {
        ERR("init encoder failed: load VPE so error");
        return VIDEO_ENCODER_INIT_FAIL;
    }

    if (!CreateVpeDevice()) {
        ERR("failed to init vpe device");
        return VIDEO_ENCODER_INIT_FAIL;
    }

    do {
        if (!InitVpeFilter()) {
            ERR("failed to init vpe filter");
            break;
        }
        m_frame.width = m_width;
        m_frame.height = m_height;
        m_filterFrame = new (std::nothrow) VpiFrame();
        if (m_filterFrame == nullptr) {
            ERR("failed to new frame buffer");
            break;
        }
        for (uint32_t i = 0; i < NUM_OF_PLANES; ++i) {
            m_frame.linesize[i] = (i == 0) ? m_width : m_width / COMPRESS_RATIO;
            int iFrameSize = ( i==0 ) ? m_width * m_height : m_width * m_height >> COMPRESS_RATIO;
            m_frame.data[i] = new (std::nothrow) uint8_t[iFrameSize];
            if (m_frame.data[i] == nullptr) {
                ERR("failed to new frame buffer");
                break;
            }
        }
        INFO("init encoder success");
        return VIDEO_ENCODER_SUCCESS;
    } while (false);

    DestroyEncoder();
    return VIDEO_ENCODER_INIT_FAIL;

}

bool VideoEncoderVpe::VerifyEncodeParams(const EncodeParams &encParams)
{
    if (encParams.width > VPE_WIDTH_MAX || encParams.height > VPE_HEIGHT_MAX ||
        encParams.width < VPE_WIDTH_MIN || encParams.height < VPE_HEIGHT_MIN) {
        ERR("resolution [%ux%u] is not supported", encParams.width, encParams.height);
        return false;
    }
    if (encParams.frameRate != FRAMERATE_MIN && encParams.frameRate != FRAMERATE_MAX) {
        ERR("framerate [%u] is not supported", encParams.frameRate);
        return false;
    }
    if (encParams.bitrate < BITRATE_MIN || encParams.bitrate > BITRATE_MAX) {
        ERR("bitrate [%u] is not supported", encParams.bitrate);
        return false;
    }
    if (encParams.gopSize < GOPSIZE_MIN || encParams.gopSize > GOPSIZE_MAX) {
        ERR("gopsize [%u] is not supported", encParams.gopSize);
        return false;
    }
    if (encParams.profile != ENCODE_PROFILE_BASELINE &&
        encParams.profile != ENCODE_PROFILE_MAIN &&
        encParams.profile != ENCODE_PROFILE_HIGH) {
        ERR("profile [%u] is not supported", encParams.profile);
        return false;
    }
    INFO("width:%u, height:%u, framerate:%u, bitrate:%u, gopsize:%u, profile:%u", encParams.width, encParams.height,
        encParams.frameRate, encParams.bitrate, encParams.gopSize, encParams.profile);
    return true;
}

bool VideoEncoderVpe::LoadVpeSharedLib()
{
    if (g_vpeLoaded) {
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
    g_vpeLoaded = true;
    return true;
}

EncoderRetCode VideoEncoderVpe::StartEncoder()
{
    INFO("start encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderVpe::EncodeOneFrame(const uint8_t *inputData, uint32_t inputSize,
    uint8_t **outputData, uint32_t *outputSize)
{
    uint32_t frameSize = static_cast<uint32_t>(m_width * m_height * NUM_OF_PLANES / COMPRESS_RATIO);
    if (inputSize < frameSize) {
        ERR("input size error: size(%u) < frame size(%u)", inputSize, frameSize);
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

    VpeCodecCtx *encCtx = &m_vpeContext.encCtx;
    VpiFrame *vpiFrame = nullptr;
    VpiCtrlCmdParam cmd = {.cmd = VPI_CMD_ENC_GET_EMPTY_FRAME_SLOT, .data = nullptr};
    if (encCtx->vpi->control(encCtx->ctx, &cmd, &vpiFrame) != 0 || vpiFrame == nullptr) {
        ERR("get empty frame slot failed");
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    memcpy(vpiFrame, m_filterFrame, sizeof(VpiFrame));

    vpiFrame->opaque = vpiFrame->vpi_opaque = m_filterFrame;
    int ret = encCtx->vpi->encode_put_frame(encCtx->ctx, vpiFrame);
    if (ret != 0) {
        ERR("encode_put_frame failed, error=(%d)", ret);
        return VIDEO_ENCODER_ENCODE_FAIL;
    }
    m_frameCount++;
    int streamSize = 0;
    while (ret == 0) {
        cmd.cmd = VPI_CMD_ENC_GET_FRAME_PACKET;
        if (encCtx->vpi->control(encCtx->ctx, &cmd, &streamSize) == -1) {
            break;
        }
        m_streamBuffer.reserve((streamSize + 0xFFFF) & (~0xFFFF));
        m_packet.data = m_streamBuffer.data();
        m_packet.size = streamSize;
        ret = encCtx->vpi->encode_get_packet(encCtx->ctx, &m_packet);
        if (ret != 0) {
            ERR("encode_get_pakcet failed, error=(%d)", ret);
            return VIDEO_ENCODER_ENCODE_FAIL;
        }
        *outputData = static_cast<uint8_t*>(m_packet.data);
        *outputSize = static_cast<uint32_t>(m_packet.size);
    }
    return VIDEO_ENCODER_SUCCESS;
}

bool VideoEncoderVpe::InitFrameData(const uint8_t *src)
{
    if (src == nullptr) {
        ERR("input data buffer is null");
        return false;
    }
    const uint32_t ylength = m_width * m_height;
    const uint32_t uvlength = ylength >> COMPRESS_RATIO;
    m_frame.key_frame = 1;
    m_frame.pts = m_frameCount;
    m_frame.pkt_dts = m_frameCount;
    memcpy(m_frame.data[Y_INDEX], src, ylength);
    memcpy(m_frame.data[U_INDEX], src + ylength, uvlength);
    memcpy(m_frame.data[V_INDEX], src + ylength + uvlength, uvlength);

    // push frame to filter
    VpePPCtx *ppCtx = &m_vpeContext.ppCtx;
    if (ppCtx->vpi->process(ppCtx->ctx, &m_frame, m_filterFrame) != 0) {
        ERR("vpe_pp failed");
        return false;
    }
    if (!m_vpeContext.encCtx.initialized) {
        if (!InitVpeCodec()) {
            ERR("init vpe encoder failed");
            return false;
        }
    }

    return true;
}

EncoderRetCode VideoEncoderVpe::StopEncoder()
{
    INFO("stop encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

void VideoEncoderVpe::DestroyEncoder()
{
    INFO("destroy encoder start");
    if (g_libHandle == nullptr) {
        WARN("encoder has been destroyed");
        return;
    }
    FlushVpeCodec();
    CloseVpeCodec();
    CloseVpeFilter();
    DestoryVpeDevice();
    for (uint32_t i = 0; i < NUM_OF_PLANES; ++i) {
        if (m_frame.data[i] != nullptr) {
            delete [] m_frame.data[i];
            m_frame.data[i] = nullptr;
        }
    }
    if (m_filterFrame != nullptr) {
        delete m_filterFrame;
        m_filterFrame = nullptr;
    }
    memset(&m_vpeContext, 0, sizeof(VpeContext));
    INFO("destroy encoder done");
}

EncoderRetCode VideoEncoderVpe::ResetEncoder()
{
    INFO("resetting encoder");

    INFO("reset encoder success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderVpe::ForceKeyFrame()
{
    INFO("force key frame success");
    return VIDEO_ENCODER_SUCCESS;
}

EncoderRetCode VideoEncoderVpe::SetEncodeParams(const EncodeParams &encParams)
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

uint32_t VpeSrm::GetDeviceNumbers()
{
    struct dirent **nameList = nullptr;
    uint32_t count = 0;
    int n = scandir("/dev/", &nameList, 0, alphasort);
    while (n--) {
        if (strstr(nameList[n]->d_name, DEV_NAME_PREFIX.c_str()) != nullptr) {
            ++count;
        }
        free(nameList[n]);
    }
    free(nameList);
    return count;
}

bool VpeSrm::GetPowerState(uint32_t devId, SrmDriverStatus &status)
{
    INFO("GetPowerState In ...");
    std::string file = INFO_PATH_PREFIX + std::to_string(devId) + "/" + POWER_STATUS;
    FILE *fp = fopen(file.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    if (fscanf(fp, "%d", &status.powerState) == EOF) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    file = "/dev/disabled_transcoder" + std::to_string(devId);
    if (access(file.c_str(), F_OK) != -1) {
        status.powerState = -1;
    }
    INFO("get device %d power state: %d", devId, status.powerState);
    return true;
}

bool VpeSrm::GetEncUsage(uint32_t devId, SrmDriverStatus &status)
{
    INFO("GetEncUsage In ...");
    std::string file = INFO_PATH_PREFIX + std::to_string(devId) + "/" + ENC_UTIL;
    FILE *fp = fopen(file.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    if (fscanf(fp, "%d", &status.encUsage) == EOF) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    INFO("get device %d encode usage: %d", devId, status.encUsage);
    return true;
}

bool VpeSrm::GetMemUsage(uint32_t devId, SrmDriverStatus &status)
{
    INFO("GetMemUsage In ...");
    std::string file = INFO_PATH_PREFIX + std::to_string(devId) + "/" + MEM_USAGE;
    FILE *fp = fopen(file.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }
    constexpr int bufferSize = 4;
    char s0[bufferSize], s1[bufferSize];
    int usedS0, usedS1, freeS0, freeS1;
    if (fscanf(fp, "%s%d%*s%*s%d%*c%*s%*s%*d%*c%*s%*s", s0, &usedS0, &freeS0) == EOF ||
        fscanf(fp, "%s%d%*s%*s%d%*c%*s%*s%*d%*c%*s%*s", s1, &usedS1, &freeS1) == EOF) {
        ERR("read memory usage file error");
        fclose(fp);
        return false;
    }
    s0[bufferSize - 1] = '\0';
    s1[bufferSize - 1] = '\0';
    if (strncmp(s0, "S0:", bufferSize - 1) != 0 || strncmp(s1, "S1:", bufferSize - 1) != 0) {
        ERR("memory usage file %s format is wrong, s0=%s, freeS0=%d", file.c_str(), s0, freeS0);
        fclose(fp);
        return false;
    }
    status.freeMem = freeS0 + freeS1;
    status.usedMem = usedS0 + usedS1;
    INFO("get device %d freeMem: %dMB, usedMem: %dMB", devId, status.freeMem, status.usedMem);
    fclose(fp);
    return true;
}

bool VpeSrm::ReadDriverStatus(SrmContext &srm)
{
    INFO("ReadDriverStatus In ...");
    memset(srm.driverStatus, 0, sizeof(SrmDriverStatus) * srm.driverNums);
    for (uint32_t i = 0; i < srm.driverNums; ++i) {
        srm.driverStatus[i].devId = i;
        if (!GetMemUsage(i, srm.driverStatus[i]) || !GetEncUsage(i, srm.driverStatus[i]) ||
            !GetPowerState(i, srm.driverStatus[i])) {
            return false;
        }
    }
    return true;
}

int VpeSrm::SrmAllocateResource()
{
    SrmContext srm = {};
    srm.driverNums = GetDeviceNumbers();
    if (srm.driverNums <= 0) {
        ERR("no devices can be found, please install driver");
        return -1;
    }
    INFO("driver nums: %d", srm.driverNums);
    auto driverStatus = std::make_unique<SrmDriverStatus[]>(srm.driverNums);
    srm.driverStatus = driverStatus.get();

    if (!ReadDriverStatus(srm)) {
        ERR("read driver status failed");
        return -1;
    }

    std::vector<int> cards;
    for (uint32_t i = 0; i < srm.driverNums; ++i) {
        SrmDriverStatus &status = srm.driverStatus[i];
        if (status.powerState >= 0 && status.freeMem >= MEM_ALLOCATION_RESERVE) {
            cards.push_back(i);
            INFO("memory valid device: %d", i);
        }
    }

    int minUsage = -1;
    int bestCard = -1;
    for (const auto &card : cards) {
        int encUsage = srm.driverStatus[card].encUsage;
        if (encUsage < minUsage || minUsage < 0) {
            minUsage = encUsage;
            bestCard = card;
        }
    }

    INFO("best device: %d, encUsage: %d, usedMem: %dMB, freeMem: %dMB", bestCard, srm.driverStatus[bestCard].encUsage,
         srm.driverStatus[bestCard].usedMem, srm.driverStatus[bestCard].freeMem);
    return bestCard;
}
