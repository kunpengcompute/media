cmake_minimum_required(VERSION 3.6)
project(Common)

set(Srcs
    log/MediaLog.cpp
    log/MediaLogManager.cpp
)

add_library(${PROJECT_NAME} STATIC ${Srcs})

message("${PROJECT_NAME} enable fortify source")
target_compile_options(${PROJECT_NAME} PUBLIC
    -Wformat -Wall -fstack-protector-strong --param ssp-buffer-size=4 -D_FORTIFY_SOURCE=2 -O2 -fPIC
)
target_link_libraries(${PROJECT_NAME} PUBLIC
    -Wformat -Wl,-s -Wl,--build-id=none -Wl,-z,relro -Wl,-z,now
)
