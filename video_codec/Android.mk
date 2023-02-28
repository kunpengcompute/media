LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ROOT_DIR := $(LOCAL_PATH)/..
COMMON_DIR := $(ROOT_DIR)/common
VENDOR_DIR := $(ROOT_DIR)/vendor

LOCAL_CFLAGS := -Wformat -Wall -fstack-protector-strong --param ssp-buffer-size=4 -fPIE -D_FORTIFY_SOURCE=2 -O2 -fPIC
LOCAL_CPPFLAGS := -Wformat -Wall -fstack-protector-strong --param ssp-buffer-size=4 -fPIE -D_FORTIFY_SOURCE=2 -O2 -fPIC
LOCAL_LDFLAGS := -Wformat -Wl,--build-id=none -Wl,-z,relro -fPIE -Wl,-z,now,-z,noexecstack

LOCAL_SRC_FILES := \
    VideoCodecApi.cpp \
    VideoEncoderOpenH264.cpp \
    VideoEncoderNetint.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    system/core/liblog/include \
    $(COMMON_DIR)/log \
    $(VENDOR_DIR)/openh264 \
    $(VENDOR_DIR)/netint

LOCAL_SHARED_LIBRARIES := \
    liblog

LOCAL_STATIC_LIBRARIES := \
    libCommon

LOCAL_MODULE := libVideoCodec

LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
