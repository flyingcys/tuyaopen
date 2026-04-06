##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
set(APP_SRCS
    ${APP_PATH}/src/tuya_main.c
    ${APP_PATH}/src/cli_cmd.c
    ${APP_PATH}/src/xiaozhi_identity.c
    ${APP_PATH}/src/xiaozhi_settings.c
    ${APP_PATH}/src/xiaozhi_system.c
    ${APP_PATH}/src/xiaozhi_protocol.c
    ${APP_PATH}/src/xiaozhi_mcp.c
    ${APP_PATH}/src/xiaozhi_ota.c
    ${APP_PATH}/src/xiaozhi_upgrade.c
    ${APP_PATH}/../../src/peripherals/audio_codecs/tdl_audio/src/tdl_audio_manage.c
    ${APP_PATH}/src/xiaozhi_ws.c
    ${APP_PATH}/src/xiaozhi_ws_handshake.c
    ${APP_PATH}/src/audio/xz_portaudio_device.c
    ${APP_PATH}/src/audio/xz_audio_opus_bridge.c
    ${APP_PATH}/src/wakeword/xz_snowboy_runner.cc
    ${APP_PATH}/src/third_party/snowboy_wrapper/snowboy-detect-c-wrapper.cc
    ${APP_PATH}/src/xiaozhi_audio_linux.c
    ${APP_PATH}/src/xiaozhi_state.c
    ${APP_PATH}/src/xiaozhi_mqtt_udp.c
    ${APP_PATH}/src/xiaozhi_app.c
)

# APP_INC
set(APP_INC
    ${APP_PATH}/src
    ${APP_PATH}/../../src/peripherals/audio_codecs/tdl_audio/include
    ${APP_PATH}/src/third_party/snowboy_wrapper
    ${APP_PATH}/src/third_party/snowboy_wrapper/include
)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

if(TOS_PROJECT_PLATFORM STREQUAL "LINUX")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    find_package(BLAS REQUIRED)
    find_package(LAPACK REQUIRED)

    target_include_directories(${EXAMPLE_LIB}
        PRIVATE
            ${PORTAUDIO_INCLUDE_DIRS}
    )
    target_compile_definitions(${EXAMPLE_LIB}
        PRIVATE
            _GLIBCXX_USE_CXX11_ABI=0
    )

    set(SNOWBOY_STATIC_LIB ${APP_PATH}/src/third_party/snowboy_wrapper/lib/ubuntu64/libsnowboy-detect.a)
    set(PLATFORM_NEED_LIBS "${PLATFORM_NEED_LIBS} opus portaudio blas lapack ${SNOWBOY_STATIC_LIB}")
endif()
