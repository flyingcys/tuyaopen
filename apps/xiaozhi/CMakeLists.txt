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
    ${APP_PATH}/src/xiaozhi_ws.c
    ${APP_PATH}/src/xiaozhi_ws_handshake.c
    ${APP_PATH}/src/xiaozhi_mqtt_udp.c
    ${APP_PATH}/src/xiaozhi_app.c
)

# APP_INC
set(APP_INC
    ${APP_PATH}/src
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
