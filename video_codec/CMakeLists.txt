cmake_minimum_required(VERSION 3.6)
project(VideoCodec)

set(ROOT_REL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMMON_REL_DIR ${ROOT_REL_DIR}/common)
set(VENDOR_REL_DIR ${ROOT_REL_DIR}/vendor)

include_directories(
    ${COMMON_REL_DIR}/log
    ${VENDOR_REL_DIR}/openh264
    ${VENDOR_REL_DIR}/netint
    ${VENDOR_REL_DIR}/vpe
)

set(Srcs
    VideoCodecApi.cpp
    VideoEncoderOpenH264.cpp
    VideoEncoderNetint.cpp
    VideoEncoderVpe.cpp
)

add_library(${PROJECT_NAME} SHARED ${Srcs})

message("${PROJECT_NAME} enable fortify source")
target_compile_options(${PROJECT_NAME} PUBLIC
    -Wformat -Wall -fstack-protector-strong --param ssp-buffer-size=4 -D_FORTIFY_SOURCE=2 -O2
)
target_link_libraries(${PROJECT_NAME} PUBLIC
    Common
    -Wformat -Wl,-s -Wl,--build-id=none -Wl,-z,relro -fPIE -Wl,-z,now -Wl,-z,noexecstack
)
