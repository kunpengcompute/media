LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    video_codec/VideoCodecApi.cpp \
    video_codec/VideoEncoderOpenH264.cpp \
    video_codec/VideoEncoderNetint.cpp \
	common/log/MediaLog.cpp \
    common/log/MediaLogManager.cpp \
    common/prop/Property.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/video_codec \
    system/core/liblog/include \
    $(LOCAL_PATH)/common/log \
    $(LOCAL_PATH)/common/prop \
    $(LOCAL_PATH)/vendor/openh264 \
    $(LOCAL_PATH)/vendor/netint

LOCAL_SHARED_LIBRARIES := \
    liblog

LOCAL_CFLAGS := -Wformat -Wall -fstack-protector-strong --param ssp-buffer-size=4 -fPIE -D_FORTIFY_SOURCE=2 -O2 -fPIC
LOCAL_CPPFLAGS := -Wformat -Wall -fstack-protector-strong --param ssp-buffer-size=4 -fPIE -D_FORTIFY_SOURCE=2 -O2 -fPIC
LOCAL_LDFLAGS :=  -Wformat -Wl,--build-id=none -Wl,-z,relro -fPIE -Wl,-z,now,-z,noexecstack

LOCAL_MODULE := libVideoCodec
LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)